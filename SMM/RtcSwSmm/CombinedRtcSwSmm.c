/** @file
  COMBINED_SMM_DXE:
   - In DXE phase: registers itself into SMRAM via EFI_SMM_BASE_PROTOCOL->Register().
   - In SMM phase: locates EFI_SMM_SW_DISPATCH2_PROTOCOL and registers SW SMI callback.
   - SW SMI callback reads RTC seconds from CMOS (reg 0x00), adds 1 (wrap 0..59), writes back.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/SmmSwDispatch2.h>
#include <Protocol/SmmBase2.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include "../SmiTable.h"

STATIC EFI_HANDLE                                mSwDispatchHandle = NULL;
STATIC EFI_SMM_SW_DISPATCH2_PROTOCOL             *mSwDispatch       = NULL;
STATIC EFI_SMM_BASE2_PROTOCOL                    *mSmmBase          = NULL;

/**
  @param[in]  Bin   Binary value.
  @retval UINT8     BCD representation of Bin.
**/
STATIC
UINT8
BinToBcd (UINT8 Bin)
{
  return (UINT8)(((Bin / 10) << 4) | (Bin % 10));
}

/**
  @param[in]  Bcd   BCD value.
  @retval UINT8     Binary representation of Bcd.
**/
STATIC
UINT8
BcdToBin (UINT8 Bcd)
{
  return (UINT8)(((Bcd >> 4) * 10) + (Bcd & 0x0F));
}

/**
  Read RTC seconds.
  @retval UINT8   Raw RTC seconds value from CMOS (BCD).
**/
STATIC
UINT8
RtcReadSeconds (
  VOID
  )
{
  IoWrite8 (CMOS_INDEX_PORT, (UINT8)RTC_SECONDS_REG);
  return IoRead8 (CMOS_DATA_PORT);
}

/**
  Write RTC seconds.
  @param[in]  Seconds   Raw BCD value to write to RTC seconds register.
**/
STATIC
VOID
RtcWriteSeconds (
  IN UINT8 Seconds
  )
{
  IoWrite8 (CMOS_INDEX_PORT, (UINT8)RTC_SECONDS_REG);
  IoWrite8 (CMOS_DATA_PORT, Seconds);
}

/**
  SW SMI callback. Runs in SMM.
  Behavior:
    - Reads RTC seconds in BCD.
    - Converts to binary, increments by 1.
    - Converts back to BCD.
    - Writes to port 0x80 and commits to RTC.

  @param[in]      DispatchHandle   The unique handle assigned when registering.
  @param[in]      Context          Optional context pointer.
  @param[in,out]  CommBuffer       Optional communication buffer.
  @param[in,out]  CommBufferSize   Optional size of communication buffer.

  @retval EFI_SUCCESS   The SW SMI was handled successfully.
**/
EFI_STATUS
EFIAPI
CombinedSwSmiHandler (
  IN EFI_HANDLE   DispatchHandle,
  IN CONST VOID   *Context OPTIONAL,
  IN OUT VOID     *CommBuffer OPTIONAL,
  IN OUT UINTN    *CommBufferSize OPTIONAL
  )
{
  UINT8 Sec;
  UINT8 SecBcd;
  UINT8 NewSec;
  UINT8 NewSecBcd;

  // 1. Read raw seconds (BCD)
  SecBcd = RtcReadSeconds();
  Sec = BcdToBin (SecBcd);

  DEBUG ((DEBUG_INFO, "CombinedSwSmiHandler: read seconds BCD=0x%02x, BIN=%d\n", SecBcd, Sec));

  // 2. Increment in binary
  NewSec = (UINT8)((Sec + 1) % 60);

  // 3. Convert back to BCD for RTC
  NewSecBcd = BinToBcd (NewSec);
  IoWrite8 (PORT_80, NewSecBcd);

  // 4. Write BCD back to RTC
  RtcWriteSeconds (NewSecBcd);

  DEBUG ((DEBUG_INFO, "CombinedSwSmiHandler: wrote seconds BCD=0x%02x, BIN=%d\n", NewSecBcd, NewSec));

  return EFI_SUCCESS;
}

/**
  Register the SW SMI handler inside SMM. Must be called when executing in SMM.
  This routine:
    - Locates EFI_SMM_SW_DISPATCH2_PROTOCOL.
    - Registers CombinedSwSmiHandler with a given SwSmiInputValue.

  @retval EFI_SUCCESS   Handler registered successfully.
  @retval Others        An error occurred locating protocol or registering.
**/
STATIC
EFI_STATUS
RegisterSwHandlerInSmm (
  VOID
  )
{
  EFI_STATUS Status;
  EFI_SMM_SW_REGISTER_CONTEXT SwContext;

  if (mSwDispatch == NULL) {
    Status = gSmst->SmmLocateProtocol (
                       &gEfiSmmSwDispatch2ProtocolGuid, 
                       NULL, 
                       (VOID **)&mSwDispatch
                      );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "RegisterSwHandlerInSmm: SmmLocateProtocol(SW) failed: %r\n", Status));
      return Status;
    }
  }

  SwContext.SwSmiInputValue = SMI_TRIGGER_VALUE;

  Status = mSwDispatch->Register (
                           mSwDispatch, 
                           CombinedSwSmiHandler, 
                           &SwContext, 
                           &mSwDispatchHandle
                          );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "RegisterSwHandlerInSmm: Register failed: %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "RegisterSwHandlerInSmm: SW handler registered (value 0x%02x)\n", SwContext.SwSmiInputValue));
  }

  return Status;
}

/**
  Module entry - COMBINED_SMM_DXE.
  Behavior:
    - Locate EFI_SMM_BASE2_PROTOCOL.
    - Call InSmm() to determine whether we are currently in SMM.
    - If InSmm == TRUE:
        * Register SW SMI handler by calling RegisterSwHandlerInSmm().
    - If InSmm == FALSE:
        * We are in DXE context; SMM dispatcher will later create SMM copy.

  @param[in]  ImageHandle   The firmware-allocated handle for this image.
  @param[in]  SystemTable   Pointer to the EFI system table.

  @retval EFI_SUCCESS       Initialization succeeded.
  @retval Others            Error occurred during initialization.
**/
EFI_STATUS
EFIAPI
CombinedRtcSwSmmEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  BOOLEAN InSmm;

  //
  // Locate the SMM Base2 protocol (available both in DXE and SMM contexts)
  //
  Status = gBS->LocateProtocol (
                       &gEfiSmmBase2ProtocolGuid, 
                       NULL, 
                       (VOID **)&mSmmBase
                      );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CombinedRtcSwSmmEntry: LocateProtocol(SmmBase2) failed: %r\n", Status));
    return Status;
  }

  //
  // Query whether we're executing inside SMM.
  //
  Status = mSmmBase->InSmm (mSmmBase, &InSmm);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CombinedRtcSwSmmEntry: InSmm() failed: %r\n", Status));
    return Status;
  }

  if (InSmm) {
    //
    // We're running inside SMM (the SMM dispatcher loaded this image into SMRAM).
    // Now register our SW SMI handler.
    //
    DEBUG ((DEBUG_INFO, "CombinedRtcSwSmmEntry: Running in SMM, registering SW handler...\n"));

    Status = RegisterSwHandlerInSmm ();
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "CombinedRtcSwSmmEntry: RegisterSwHandlerInSmm() failed: %r\n", Status));
      return Status;
    }
    DEBUG ((DEBUG_INFO, "CombinedRtcSwSmmEntry: SW handler registered successfully.\n"));
  } else {
    //
    // We're in DXE phase. Nothing special to do here.
    //
    DEBUG ((DEBUG_INFO, "CombinedRtcSwSmmEntry: Running in DXE context (no SMM registration needed).\n"));
  }

  return EFI_SUCCESS;
}
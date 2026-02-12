/** @file
  DXE_SMM_DRIVER:
   - Entry runs in SMM (SMM dispatcher) and installs a Power Button SMI handler.
   - The callback reads RTC seconds via CMOS reg 0x00 and writes that byte to port 0x80.
   - Does not power off the system.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Protocol/SmmPowerButtonDispatch2.h>
#include "../SmiTable.h"

STATIC EFI_HANDLE   mPwrHandle = NULL;
STATIC EFI_SMM_POWER_BUTTON_DISPATCH2_PROTOCOL *mPowerDispatch = NULL;

/**
  Power Button SMI Callback.

  @param[in]      DispatchHandle      Handle received during register.
  @param[in]      Context             Optional context.
  @param[in,out]  CommBuffer          Optional communication buffer.
  @param[in,out]  CommBufferSize      Optional buffer size.

  @retval EFI_SUCCESS   Callback executed successfully.
**/
EFI_STATUS
EFIAPI
PowerButtonCallback (
  IN EFI_HANDLE   DispatchHandle,
  IN CONST VOID   *Context OPTIONAL,
  IN OUT VOID     *CommBuffer OPTIONAL,
  IN OUT UINTN    *CommBufferSize OPTIONAL
  )
{
  UINT8 Sec = 0;

  // Read RTC seconds via CMOS
  IoWrite8 (CMOS_INDEX_PORT, (UINT8)RTC_SECONDS_REG);
  Sec = IoRead8 (CMOS_DATA_PORT);  // Write the seconds value to port 0x80 for debug
  IoWrite8 (PORT_80, Sec);

  DEBUG ((DEBUG_INFO, "PowerButtonCallback: Power button pressed, seconds=%02x\n", Sec));

  return EFI_SUCCESS;
}

/**
  Driver Entry Point for SMM.

  @param[in]  ImageHandle   The image handle.
  @param[in]  SystemTable   System table pointer.

  @retval EFI_SUCCESS       Driver initialized successfully.
  @retval Others            Failed to install callback.
**/
EFI_STATUS
EFIAPI
PowerButtonSmmEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_SMM_POWER_BUTTON_REGISTER_CONTEXT Context;

  //
  // Locate the Power Button dispatch protocol in SMM
  //
  Status = gSmst->SmmLocateProtocol (
                          &gEfiSmmPowerButtonDispatch2ProtocolGuid, 
                          NULL, 
                          (VOID **)&mPowerDispatch
                        );
                        
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "PowerButtonSmmEntry: SmmLocateProtocol(PowerButton) failed: %r\n", Status));
    return Status;
  }

  //
  // Register callback on power button press
  //
  Context.Phase = EfiPowerButtonEntry;

  Status = mPowerDispatch->Register (
                              mPowerDispatch, 
                              PowerButtonCallback, 
                              &Context, 
                              &mPwrHandle
                            );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "PowerButtonSmmEntry: Register failed: %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "PowerButtonSmmEntry: Power Button handler registered\n"));
  return EFI_SUCCESS;
}

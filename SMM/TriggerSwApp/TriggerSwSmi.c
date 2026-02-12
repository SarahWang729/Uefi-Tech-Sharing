/** @file
  UEFI Application: 
    - Issues a Software SMI by writing a trigger value to the SMI Command Port using IoWrite8().
    - Intended to invoke the SMM SW SMI handler installed by SMM/DXE driver.
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include "../SmiTable.h"

/**
  Entry Point of UEFI Application.

  @param[in] ImageHandle    Handle of the loaded image.
  @param[in] SystemTable    Pointer to EFI system table.

  @retval EFI_SUCCESS       Application executed normally.
  @retval Others            Unexpected failure.
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  Print (L"Trigger SW SMI: 0x%02x -> port 0x%02x\n", SMI_TRIGGER_VALUE, SMI_CMD_PORT);

  IoWrite8 (SMI_CMD_PORT, (UINT8)SMI_TRIGGER_VALUE);

  Print (L"Done. SW SMI should have been signaled.\n");
  return EFI_SUCCESS;
}

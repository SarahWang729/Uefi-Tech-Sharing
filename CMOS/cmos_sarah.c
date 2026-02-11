/** @file
  CMOS Dump Shell Utility
  - Read CMOS offsets 0x00 - 0x7F via I/O ports 0x70/0x71
  - Print dump in 16-bytes-per-line format

  Key points:
  - 0x70 = Index port (bit7 controls NMI)
  - 0x71 = Data port
  - Check RTC UIP (Reg A, 0x0A bit7) to avoid inconsistent RTC bytes
**/

#include "cmos.h"
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/TimerLib.h>
#include <Protocol/CpuIo2.h>

STATIC EFI_CPU_IO2_PROTOCOL *mCpuIo2 = NULL;

STATIC
EFI_STATUS
LocateCpuIo2 (
  VOID
  )
{
  return gBS->LocateProtocol (
                &gEfiCpuIo2ProtocolGuid,
                NULL,
                (VOID **)&mCpuIo2
                );
}

STATIC
UINT8
IoRead8 (
  IN UINT16 Port
  )
{
  UINT8 Data = 0;

  // Defensive: mCpuIo2 should have been located already
  if (mCpuIo2 == NULL) {
    return 0;
  }

  mCpuIo2->Io.Read (
                mCpuIo2,
                EfiCpuIoWidthUint8,
                Port,
                1,
                &Data
                );
  return Data;
}

STATIC
VOID
IoWrite8 (
  IN UINT16 Port,
  IN UINT8  Data
  )
{
  if (mCpuIo2 == NULL) {
    return;
  }

  mCpuIo2->Io.Write (
                mCpuIo2,
                EfiCpuIoWidthUint8,
                Port,
                1,
                &Data
                );
}

STATIC
VOID
IoDelaySmall (
  VOID
  )
{
  // Minimal delay to let legacy IO settle down
  MicroSecondDelay (2);
}

//
// ---------- RTC safety: wait UIP clear ----------
// UIP = Register A (0x0A) bit7
//
STATIC
VOID
WaitForRtcUpdateComplete (
  VOID
  )
{
  UINTN Retry;
  UINT8 RegA;

  // Timeout-protected loop (avoid infinite hang)
  for (Retry = 0; Retry < 2000; Retry++) {
    // Select Register A with NMI disabled
    IoWrite8 (CMOS_INDEX_PORT, (UINT8)(CMOS_RTC_REG_A | CMOS_NMI_DISABLE_BIT));
    IoDelaySmall ();

    RegA = IoRead8 (CMOS_DATA_PORT);

    if ((RegA & CMOS_RTC_UIP_BIT) == 0) {
      break;
    }

    MicroSecondDelay (50);
  }

  // Restore index port (re-enable NMI)
  IoWrite8 (CMOS_INDEX_PORT, 0);
  IoDelaySmall ();
}

//
// ---------- Core: read one CMOS byte ----------
// Standard sequence:
//   out 0x70 <- offset | 0x80 (disable NMI)
//   in  0x71 -> data
//   out 0x70 <- 0 (restore, enable NMI)
//
STATIC
UINT8
ReadCmosByte (
  IN UINT8 Offset
  )
{
  UINT8 Data;

  IoWrite8 (CMOS_INDEX_PORT, (UINT8)(Offset | CMOS_NMI_DISABLE_BIT));
  IoDelaySmall ();

  Data = IoRead8 (CMOS_DATA_PORT);

  IoWrite8 (CMOS_INDEX_PORT, 0);
  IoDelaySmall ();

  return Data;
}

//
// ---------- Print dump (16 bytes) ----------
//
STATIC
VOID
PrintCmosDump (
  IN CONST UINT8 *Buffer,
  IN UINTN       Size,
  IN UINT8       BaseOffset
  )
{
  UINTN i, j;

  for (i = 0; i < Size; i += CMOS_BYTES_PER_LINE) {
    Print (L"%02x: ", (UINT32)(BaseOffset + i));

    for (j = 0; j < CMOS_BYTES_PER_LINE; j++) {
      if (i + j < Size) {
        Print (L"%02x ", Buffer[i + j]);
      } else {
        Print (L"   ");
      }
    }
    Print(L"\n");
  }
}

//
// ---------- EntryPoint ----------
//
EFI_STATUS
EFIAPI
CmosDumpEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  UINT8      Cmos[CMOS_DUMP_SIZE];
  UINTN      Index;

  Status = LocateCpuIo2 ();
  if (EFI_ERROR (Status) || mCpuIo2 == NULL) {
    Print (L"[CMOS] LocateProtocol(CpuIo2) failed: %r\n", Status);
    return Status;
  }

  Print (L"========= CMOS Dump (0x%02x - 0x%02x) ==========\n",
         CMOS_DUMP_START, CMOS_DUMP_END);

  for (Index = 0; Index < CMOS_DUMP_SIZE; Index++) {
    // Conservative: prevent RTC-related bytes from tearing
    WaitForRtcUpdateComplete ();

    Cmos[Index] = ReadCmosByte ((UINT8)(CMOS_DUMP_START + Index));
  }

  PrintCmosDump (Cmos, CMOS_DUMP_SIZE, CMOS_DUMP_START);

  return EFI_SUCCESS;
}
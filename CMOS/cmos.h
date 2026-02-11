/** @file
  CMOS Dump Shell Utility

  Reads CMOS registers 0x00 - 0x7F via I/O ports 0x70/0x71 and prints a hex dump.

  Notes:
  - Uses bit7 of port 0x70 to disable NMI during access (as common legacy sequence).
  - Optionally waits for RTC Update-In-Progress (UIP) to clear before reading.

**/

#ifndef _CMOS_DUMP_H_
#define _CMOS_DUMP_H_

#include <Uefi.h>

//
// CMOS legacy I/O ports
//
#define CMOS_INDEX_PORT                 0x70
#define CMOS_DATA_PORT                  0x71

//
// CMOS range for this exercise
//
#define CMOS_DUMP_START                 0x00
#define CMOS_DUMP_END                   0x7F
#define CMOS_DUMP_SIZE                  (CMOS_DUMP_END - CMOS_DUMP_START + 1)

//
// Bit definitions
//
#define CMOS_NMI_DISABLE_BIT            0x80

//
// RTC status register A (commonly used for UIP check)
//
#define CMOS_RTC_REG_A                  0x0A
#define CMOS_RTC_UIP_BIT                0x80

//
// Formatting
//
#define CMOS_BYTES_PER_LINE             16

//
// Function prototypes
//
EFI_STATUS
EFIAPI
CmosDumpEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

#endif // _CMOS_DUMP_H_

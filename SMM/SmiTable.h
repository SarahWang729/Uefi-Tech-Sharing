/** @file
  SmiTable_hw.h
  Common hardware I/O port and SMI number table for SMM homework.
**/

#ifndef _SMI_TABLE_H_
#define _SMI_TABLE_H_

//
// ---------------------------
//  Software SMI definitions
// ---------------------------
//

// Intel default software SMI command port (write SMI value here to trigger SW SMI)
#define SMI_CMD_PORT        0xB2

// Software SMI input values
#define SMI_TRIGGER_VALUE  0xC2   // Trigger CombinedRtcSwSmm (SW SMI handler)
#define PORT_80            0x80

//
// ---------------------------
//  RTC / CMOS I/O definitions
// ---------------------------
//

// Standard CMOS index and data ports
#define CMOS_INDEX_PORT     0x70
#define CMOS_DATA_PORT      0x71

// RTC register addresses
#define RTC_SECONDS_REG     0x00    // Seconds register
#define RTC_MINUTES_REG     0x02    // Minutes (optional if needed)
#define RTC_HOURS_REG       0x04    // Hours (optional if needed)

#endif

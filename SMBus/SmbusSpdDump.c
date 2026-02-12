/** @file
  Minimal SMBus SPD Dump Shell Application
  - Probe SPD addresses 0xA0..0xAE (8-bit) to find a responding device
  - Dump SPD bytes (0x00..0xFF) from the first responding address
**/

#include <Uefi.h>
#include <Protocol/SmbusHc.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>

STATIC
EFI_STATUS
ProbeFirstSpdAddress (
  IN  EFI_SMBUS_HC_PROTOCOL *Smbus,
  OUT UINT8                 *OutSpdAddr8
  )
{
  UINT8 Addr8;

  if (Smbus == NULL || OutSpdAddr8 == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Print(L"Probing SPD addresses (8-bit): 0xA0..0xAE step 2\n");

  for (Addr8 = 0xA0; Addr8 <= 0xAE; Addr8 = (UINT8)(Addr8 + 2)) {
    EFI_SMBUS_DEVICE_ADDRESS Slave;
    UINTN                    Len;
    UINT8                    Data;
    EFI_STATUS               Status;

    Slave.SmbusDeviceAddress = (UINT8)(Addr8 >> 1);  // 7-bit
    Len  = 1;
    Data = 0;

    // Try read SPD offset 0x00
    Status = Smbus->Execute(
                      Smbus,
                      Slave,
                      0x00,
                      EfiSmbusReadByte,
                      FALSE,
                      &Len,
                      &Data
                      );

    if (!EFI_ERROR(Status)) {
      Print(L"  0x%02x (7-bit 0x%02x): ACK, byte[0]=0x%02x\n", Addr8, Slave.SmbusDeviceAddress, Data);
      *OutSpdAddr8 = Addr8;
      Print(L"\n");
      return EFI_SUCCESS;
    } else {
      Print(L"  0x%02x (7-bit 0x%02x): NO (%r)\n", Addr8, Slave.SmbusDeviceAddress, Status);
    }
  }

  Print(L"\nNo SPD device responded on 0xA0..0xAE\n");
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_SMBUS_HC_PROTOCOL     *Smbus;
  EFI_SMBUS_DEVICE_ADDRESS   Slave;
  UINT8                     *Buf;
  UINTN                      i;

  // Fixed dump range
  CONST UINT16 Offset    = 0x00;
  CONST UINTN  LengthAll = 0x100;    // 256 bytes

  UINT8 SpdAddr8 = 0;               // will be set by probe
  UINT8 SpdAddr7 = 0;

  // Locate SMBus Host Controller protocol
  Status = gBS->LocateProtocol(&gEfiSmbusHcProtocolGuid, NULL, (VOID **)&Smbus);
  if (EFI_ERROR(Status) || Smbus == NULL) {
    Print(L"Locate EFI_SMBUS_HC_PROTOCOL failed: %r\n", Status);
    return Status;
  }

  // Probe and pick the first working SPD address
  Status = ProbeFirstSpdAddress(Smbus, &SpdAddr8);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  SpdAddr7 = (UINT8)(SpdAddr8 >> 1);

  // Allocate buffer
  Buf = AllocateZeroPool(LengthAll);
  if (Buf == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Header
  Print(L"Reading SPD via SMBus...\n");
  Print(L"  Address: 8-bit 0x%02x (7-bit 0x%02x)\n", SpdAddr8, SpdAddr7);
  Print(L"  Offset : 0x%02x\n", (UINT8)Offset);
  Print(L"  Length : 0x%04x (%d)\n\n", (UINT16)LengthAll, (UINT32)LengthAll);

  Slave.SmbusDeviceAddress = SpdAddr7;

  // Read 0x00..0xFF via EfiSmbusReadByte
  for (i = 0; i < LengthAll; i++) {
    UINTN  DataLen = 1;
    UINT8  Data    = 0;
    UINT8  Command = (UINT8)(Offset + i);

    // Optional: small retry to avoid transient bus busy
    UINTN Try;
    for (Try = 0; Try < 3; Try++) {
      Status = Smbus->Execute(
                        Smbus,
                        Slave,
                        Command,
                        EfiSmbusReadByte,
                        FALSE,     // PecCheck
                        &DataLen,
                        &Data
                        );
      if (!EFI_ERROR(Status)) {
        break;
      }
      gBS->Stall(2000); // 2ms
    }

    if (EFI_ERROR(Status)) {
      Print(L"Read failed at offset 0x%02x: %r\n", Command, Status);
      FreePool(Buf);
      return Status;
    }

    Buf[i] = Data;
  }

  // Dump 16 bytes per line + ASCII
  for (i = 0; i < LengthAll; i += 16) {
    UINTN j;

    Print(L"%04x: ", (UINT16)(Offset + i));

    for (j = 0; j < 16; j++) {
      Print(L"%02x ", Buf[i + j]);
    }

    Print(L"\n");
  }

  FreePool(Buf);
  return EFI_SUCCESS;
}

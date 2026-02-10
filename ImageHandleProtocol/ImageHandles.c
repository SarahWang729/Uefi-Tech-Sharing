/** @file
This sample application bases on Image Handles utillity library provided by EDK II,
which lets users:
1. List all handles in the system
2. Search handles by protocol GUID
3. Search handles by protocol Name
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/HandleParsingLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleFileSystem.h>

//
// Forward declarations
//
VOID DumpAllHandles(VOID);
VOID SearchByProtocolGuid(VOID);
VOID SearchByProtocolName(VOID);
VOID PrintProtocolsOnHandle(EFI_HANDLE Handle);

//
// Global variables
//
extern EFI_BOOT_SERVICES *gBS;
extern EFI_SYSTEM_TABLE *gST;

//
// Parse GUID string in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
//
EFI_STATUS
ParseGuidString (
  IN  CHAR16 *GuidStr,
  OUT EFI_GUID *Guid
  )
{
  UINTN i, j, len;
  CHAR16 temp[3];
  UINT8 bytes[16];

  if (GuidStr == NULL || Guid == NULL)
    return EFI_INVALID_PARAMETER;

  len = StrLen(GuidStr);
  if (len != 36)  // must be xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    return EFI_INVALID_PARAMETER;

  //
  // Remove dashes and convert each hex pair to a byte
  //
  j = 0;
  for (i = 0; i < len && j < 16; ) {
    if (GuidStr[i] == L'-') {
      i++;
      continue;
    }
    if (i + 1 >= len) {
      return EFI_INVALID_PARAMETER;
    }
    temp[0] = GuidStr[i];
    temp[1] = GuidStr[i + 1];
    temp[2] = 0;
    bytes[j++] = (UINT8)StrHexToUintn(temp);
    i += 2;
  }

  if (j != 16)
      return EFI_INVALID_PARAMETER;

  //
  // Assign bytes to GUID fields
  //
  Guid->Data1 = ((UINT32)bytes[0] << 24) | ((UINT32)bytes[1] << 16) |
                ((UINT32)bytes[2] << 8) | ((UINT32)bytes[3]);
  Guid->Data2 = ((UINT16)bytes[4] << 8) | bytes[5];
  Guid->Data3 = ((UINT16)bytes[6] << 8) | bytes[7];
  CopyMem(Guid->Data4, &bytes[8], 8);

  return EFI_SUCCESS;
}

//
// Dump all handles in the system
//
VOID
DumpAllHandles(VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuffer;
  UINTN HandleCount;
  UINTN Index;

  Print(L"\n=== Dumping All Handles ===\n");

  //
  // Get all handles in the system
  //
  Status = gBS->LocateHandleBuffer(
                  AllHandles,
                  NULL,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );

  if (EFI_ERROR(Status)) {
    Print(L"Error locating handles: %r\n", Status);
    return;
  }

  //
  // Print information for each handle
  //
  for (Index = 0; Index < HandleCount; Index++) {
    Print(L"Handle %d <%p>:\n", Index, HandleBuffer[Index]);
    PrintProtocolsOnHandle(HandleBuffer[Index]);
    Print(L"\n");
  }

  Print(L"Total handles: %d\n", HandleCount);

  // Free the buffer
  if (HandleBuffer != NULL) {
    FreePool(HandleBuffer);
  }
}

//
// Search handles by Protocol GUID
//
VOID
SearchByProtocolGuid(VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuffer;
  UINTN HandleCount;
  UINTN Index;
  CHAR16 GuidStr[50];
  EFI_GUID SearchGuid;
  EFI_INPUT_KEY Key;
  UINTN EventIndex;
  CHAR16 *ProtocolName;

  Print(L"\n=== Search Handle by Protocol GUID ===\n");
  Print(L"Enter GUID (format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx): ");
  
  //
  // Read GUID input
  //
  ZeroMem(GuidStr, sizeof(GuidStr));
  for (Index = 0; Index < 36; Index++) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    GuidStr[Index] = Key.UnicodeChar;
    Print(L"%c", Key.UnicodeChar);
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      GuidStr[Index] = L'\0';
      break;
    }
  }
  Print(L"\n");

  //
  // Parse GUID string
  //
  Status = ParseGuidString(GuidStr, &SearchGuid);
  if (EFI_ERROR(Status)) {
    Print(L"Invalid GUID format!\n");
    return;
  }

  //
  // Get protocol name for display
  //
  ProtocolName = GetStringNameFromGuid(&SearchGuid, NULL);

  Print(L"Searching for protocol: %s (%g)\n", ProtocolName, &SearchGuid);

  //
  // Search handles by protocol
  //
  Status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  &SearchGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );

  if (EFI_ERROR(Status) || HandleCount == 0) {
    Print(L"No handles found for the specified GUID\n");
    return;
  }

  Print(L"Found %d handle(s) supporting this protocol:\n", HandleCount);
  
  for (Index = 0; Index < HandleCount; Index++) {
    Print(L"Handle %d <%p>:\n", Index, HandleBuffer[Index]);
    PrintProtocolsOnHandle(HandleBuffer[Index]);
    Print(L"\n");
  }

  if (HandleBuffer != NULL) {
    FreePool(HandleBuffer);
  }
}

//
// Search handles by Protocol Name
//
VOID
SearchByProtocolName(VOID)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuffer;
  UINTN HandleCount;
  UINTN Index;
  CHAR16 ProtocolName[50];
  EFI_GUID *SearchGuid;
  EFI_INPUT_KEY Key;
  UINTN EventIndex;

  Print(L"\n=== Search Handle by Protocol Name ===\n");
  Print(L"Enter Protocol Name: ");
  
  //
  // Read protocol name input
  //
  ZeroMem(ProtocolName, sizeof(ProtocolName));
  for (Index = 0; Index < 49; Index++) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      ProtocolName[Index] = L'\0';
      break;
    }
    ProtocolName[Index] = Key.UnicodeChar;
    Print(L"%c", Key.UnicodeChar);
  }
  Print(L"\n");

  //
  // Convert protocol name to GUID
  //
  Status = GetGuidFromStringName(ProtocolName, NULL, &SearchGuid);
  if (EFI_ERROR(Status)) {
    Print(L"Unknown protocol name: %s\n", ProtocolName);
    return;
  }

  Print(L"Searching for protocol: %s (%g)\n", ProtocolName, SearchGuid);

  //
  // Search handles by protocol
  //
  Status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  SearchGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );

  if (EFI_ERROR(Status) || HandleCount == 0) {
    Print(L"No handles found for the specified protocol\n");
    if (SearchGuid != NULL) {
      FreePool(SearchGuid);
    }
    return;
  }

  Print(L"Found %d handle(s) supporting this protocol:\n", HandleCount);
  
  for (Index = 0; Index < HandleCount; Index++) {
    Print(L"Handle %d <%p>:\n", Index, HandleBuffer[Index]);
    PrintProtocolsOnHandle(HandleBuffer[Index]);
    Print(L"\n");
  }

  //
  // Clean up
  //
  if (HandleBuffer != NULL) {
    FreePool(HandleBuffer);
  }
  if (SearchGuid != NULL) {
    FreePool(SearchGuid);
  }
}

//
// Print all protocols on a specific handle
//
VOID
PrintProtocolsOnHandle(EFI_HANDLE Handle)
{
  EFI_STATUS Status;
  EFI_GUID **ProtocolBuffer;
  UINTN ProtocolBufferCount;
  UINTN Index;
  CHAR16 *ProtocolName;

  if (Handle == NULL) {
    return;
  }

  //
  // Get all protocols on this handle
  //
  Status = gBS->ProtocolsPerHandle(
                  Handle,
                  &ProtocolBuffer,
                  &ProtocolBufferCount
                  );

  if (EFI_ERROR(Status)) {
    Print(L"    Error getting protocols: %r\n", Status);
    return;
  }

  //
  // Print each protocol
  //
  for (Index = 0; Index < ProtocolBufferCount; Index++) {
    ProtocolName = GetStringNameFromGuid(ProtocolBuffer[Index], NULL);
    Print(L"    %s\n", ProtocolName);
    Print(L"        GUID: %g\n", ProtocolBuffer[Index]);
  }

  //
  // Free the protocol buffer
  //
  if (ProtocolBuffer != NULL) {
    FreePool(ProtocolBuffer);
  }
}

//
// Main entry point
//
INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  EFI_STATUS Status;
  UINTN MenuChoice;
  EFI_INPUT_KEY Key;
  UINTN EventIndex;

  MenuChoice = 0;

  while (TRUE) {
    // Clear screen and show menu
    Print(L"\n\n=== UEFI Handle Explorer ===\n");
    Print(L"[1] All Handle List\n");
    Print(L"[2] Search Handle by Protocol GUID\n");
    Print(L"[3] Search Handle by Protocol Name\n");
    Print(L"[4] Exit\n\n");

    Print(L"Select option (1-4): ");
    
    // Read input using WaitForEvent
    Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    if (!EFI_ERROR(Status)) {
      Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
      if (!EFI_ERROR(Status)) {
        MenuChoice = Key.UnicodeChar - L'0';
      }
    }
    
    if (EFI_ERROR(Status)) {
      Print(L"Error reading input.\n");
      continue;
    }

    switch (MenuChoice) {
      case 1:
        DumpAllHandles();
        break;
      case 2:
        SearchByProtocolGuid();
        break;
      case 3:
        SearchByProtocolName();
        break;
      case 4:
        Print(L"Exiting Handle Explorer...\n");
        return EFI_SUCCESS;
      default:
        Print(L"Invalid option! Please select 1-4.\n");
        break;
    }

    Print(L"\nPress any key to continue...");
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  }

  return EFI_SUCCESS;
}
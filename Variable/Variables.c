#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiApplicationEntryPoint.h>

#define EFI_VARIABLE_NON_VOLATILE           0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS     0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS         0x00000004

//
// Print GUID helper
//
VOID
PrintGuid (
  IN EFI_GUID *Guid
  )
{
  Print(L"%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
         Guid->Data1, Guid->Data2, Guid->Data3,
         Guid->Data4[0], Guid->Data4[1],
         Guid->Data4[2], Guid->Data4[3],
         Guid->Data4[4], Guid->Data4[5],
         Guid->Data4[6], Guid->Data4[7]);
}

//
// ReadLine helper
//
EFI_STATUS
ReadLine (
  OUT CHAR16 *Buffer,
  IN  UINTN   BufferLen
  )
{
  UINTN Index;
  EFI_INPUT_KEY Key;
  UINTN EventIndex;

  Index = 0;
  ZeroMem(Buffer, BufferLen * sizeof(CHAR16));

  while (TRUE) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Print(L"\r\n");
      Buffer[Index] = L'\0';
      return EFI_SUCCESS;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Index > 0) {
        Index--;
        Print(L"\b \b");
      }
      continue;
    }

    if (Key.UnicodeChar >= 0x20 && Key.UnicodeChar <= 0x7E) {
      if (Index + 1 < BufferLen) {
        Buffer[Index++] = Key.UnicodeChar;
        Print(L"%c", Key.UnicodeChar);
      }
    }
  }
}

//
// Parse GUID string like "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
//
EFI_STATUS
ParseGuidString (
  IN  CHAR16   *GuidStr,
  OUT EFI_GUID *Guid
  )
{
  UINTN i, j, len;
  CHAR16 temp[3];
  UINT8 bytes[16];

  if (GuidStr == NULL || Guid == NULL)
    return EFI_INVALID_PARAMETER;

  len = StrLen(GuidStr);
  if (len != 36)
    return EFI_INVALID_PARAMETER;

  j = 0;
  for (i = 0; i < len && j < 16;) {
    if (GuidStr[i] == L'-') {
      i++;
      continue;
    }
    if (i + 1 >= len)
      return EFI_INVALID_PARAMETER;

    temp[0] = GuidStr[i];
    temp[1] = GuidStr[i + 1];
    temp[2] = 0;
    bytes[j++] = (UINT8)StrHexToUintn(temp);
    i += 2;
  }

  if (j != 16)
    return EFI_INVALID_PARAMETER;

  Guid->Data1 = ((UINT32)bytes[0] << 24) |
                ((UINT32)bytes[1] << 16) |
                ((UINT32)bytes[2] << 8)  |
                ((UINT32)bytes[3]);
  Guid->Data2 = ((UINT16)bytes[4] << 8) | bytes[5];
  Guid->Data3 = ((UINT16)bytes[6] << 8) | bytes[7];
  CopyMem(Guid->Data4, &bytes[8], 8);

  return EFI_SUCCESS;
}

//
// List all variables
//
EFI_STATUS
ListAllVariables (
  VOID
  )
{
  EFI_STATUS Status;
  UINTN NameSize;
  CHAR16 *Name;
  EFI_GUID Guid;
  UINTN OldSize;
  CHAR16 *NewBuf;

  NameSize = 128;
  Name = AllocateZeroPool(NameSize * sizeof(CHAR16));
  if (Name == NULL)
    return EFI_OUT_OF_RESOURCES;

  Print(L"\n=== Listing All Variables ===\n");

  while (TRUE) {
    OldSize = NameSize;
    Status = gRT->GetNextVariableName(&NameSize, Name, &Guid);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      NewBuf = ReallocatePool(OldSize * sizeof(CHAR16),
                              NameSize * sizeof(CHAR16),
                              Name);
      if (NewBuf == NULL) {
        FreePool(Name);
        return EFI_OUT_OF_RESOURCES;
      }
      Name = NewBuf;
      continue;
    }

    if (Status == EFI_NOT_FOUND)
      break;

    if (EFI_ERROR(Status)) {
      Print(L"Error: %r\n", Status);
      break;
    }

    Print(L"Name: %s  GUID: ", Name);
    PrintGuid(&Guid);
    Print(L"\n");
  }

  FreePool(Name);
  return EFI_SUCCESS;
}

//
// Search variable by name substring
//
EFI_STATUS
SearchVariableByName (
  VOID
  )
{
  EFI_STATUS Status;
  CHAR16 Input[100];
  EFI_GUID Guid;
  UINTN NameSize;
  CHAR16 *Name;
  BOOLEAN Found;
  UINTN OldSize;
  CHAR16 *NewBuf;

  Print(L"\nEnter variable name substring: ");
  ReadLine(Input, 100);

  NameSize = 128;
  Name = AllocateZeroPool(NameSize * sizeof(CHAR16));
  Found = FALSE;

  while (TRUE) {
    OldSize = NameSize;
    Status = gRT->GetNextVariableName(&NameSize, Name, &Guid);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      NewBuf = ReallocatePool(OldSize * sizeof(CHAR16),
                              NameSize * sizeof(CHAR16),
                              Name);
      if (NewBuf == NULL)
        return EFI_OUT_OF_RESOURCES;
      Name = NewBuf;
      continue;
    }

    if (Status == EFI_NOT_FOUND)
      break;

    if (EFI_ERROR(Status))
      break;

    if (StrStr(Name, Input) != NULL) {
      Found = TRUE;
      Print(L"Found: %s  GUID: ", Name);
      PrintGuid(&Guid);
      Print(L"\n");
    }
  }

  if (!Found)
    Print(L"No match found.\n");

  FreePool(Name);
  return EFI_SUCCESS;
}

//
// Search variable by GUID
//
EFI_STATUS
SearchVariableByGuid (
  VOID
  )
{
  CHAR16 Input[50];
  EFI_GUID Target;
  EFI_STATUS Status;
  UINTN NameSize;
  CHAR16 *Name;
  EFI_GUID Guid;
  BOOLEAN Found;
  UINTN OldSize;
  CHAR16 *NewBuf;

  Print(L"\nEnter GUID to search (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx): ");
  ReadLine(Input, 50);

  Status = ParseGuidString(Input, &Target);
  if (EFI_ERROR(Status)) {
    Print(L"Invalid GUID format.\n");
    return EFI_INVALID_PARAMETER;
  }

  NameSize = 128;
  Name = AllocateZeroPool(NameSize * sizeof(CHAR16));
  Found = FALSE;

  while (TRUE) {
    OldSize = NameSize;
    Status = gRT->GetNextVariableName(&NameSize, Name, &Guid);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      NewBuf = ReallocatePool(OldSize * sizeof(CHAR16),
                              NameSize * sizeof(CHAR16),
                              Name);
      if (NewBuf == NULL)
        return EFI_OUT_OF_RESOURCES;
      Name = NewBuf;
      continue;
    }

    if (Status == EFI_NOT_FOUND)
      break;

    if (EFI_ERROR(Status))
      break;

    if (CompareMem(&Guid, &Target, sizeof(EFI_GUID)) == 0) {
      Found = TRUE;
      Print(L"Found: %s\n", Name);
    }
  }

  if (!Found)
    Print(L"No variables found for that GUID.\n");

  FreePool(Name);
  return EFI_SUCCESS;
}

//
// Create a new variable
//
EFI_STATUS
CreateNewVariable (
  VOID
  )
{
  CHAR16 Name[100];
  CHAR16 GuidStr[50];
  CHAR16 DataStr[200];
  CHAR16 AttrStr[5];
  EFI_GUID Guid;
  UINT32 Attr;
  EFI_STATUS Status;
  UINTN DataSize;

  Print(L"\nEnter new variable name: ");
  ReadLine(Name, 100);

  Print(L"Enter GUID: ");
  ReadLine(GuidStr, 50);

  Status = ParseGuidString(GuidStr, &Guid);
  if (EFI_ERROR(Status)) {
    Print(L"Invalid GUID format.\n");
    return EFI_INVALID_PARAMETER;
  }

  Print(L"Enter attributes (1=BS, 2=BS+NV, 3=RT+BS, 4=RT+BS+NV): ");
  ReadLine(AttrStr, 5);

  if (AttrStr[0] == L'1')
    Attr = EFI_VARIABLE_BOOTSERVICE_ACCESS;
  else if (AttrStr[0] == L'2')
    Attr = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE;
  else if (AttrStr[0] == L'3')
    Attr = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
  else
    Attr = EFI_VARIABLE_BOOTSERVICE_ACCESS |
           EFI_VARIABLE_RUNTIME_ACCESS |
           EFI_VARIABLE_NON_VOLATILE;

  Print(L"Enter data string: ");
  ReadLine(DataStr, 200);

  DataSize = StrLen(DataStr) * sizeof(CHAR16);
  Status = gRT->SetVariable(Name, &Guid, Attr, DataSize, DataStr);

  if (EFI_ERROR(Status))
    Print(L"SetVariable failed: %r\n", Status);
  else
    Print(L"Variable created successfully.\n");

  return Status;
}

//
// Delete variable
//
EFI_STATUS
DeleteVariable (
  VOID
  )
{
  CHAR16 Name[100];
  CHAR16 GuidStr[50];
  EFI_GUID Guid;
  EFI_STATUS Status;

  Print(L"\nEnter variable name to delete: ");
  ReadLine(Name, 100);

  Print(L"Enter GUID: ");
  ReadLine(GuidStr, 50);

  Status = ParseGuidString(GuidStr, &Guid);
  if (EFI_ERROR(Status)) {
    Print(L"Invalid GUID format.\n");
    return EFI_INVALID_PARAMETER;
  }

  Status = gRT->SetVariable(Name, &Guid, 0, 0, NULL);

  if (EFI_ERROR(Status))
    Print(L"Delete failed: %r\n", Status);
  else
    Print(L"Variable deleted successfully.\n");

  return Status;
}

//
// Main Menu
//
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_INPUT_KEY Key;
  UINTN EventIndex;

  while (TRUE) {
    Print(L"\n=== UEFI Variable Management Tool ===\n");
    Print(L"1. List all variables\n");
    Print(L"2. Search variable by name\n");
    Print(L"3. Search variable by GUID\n");
    Print(L"4. Create new variable\n");
    Print(L"5. Delete variable\n");
    Print(L"6. Exit\n");
    Print(L"Choose option: ");

    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (Key.UnicodeChar == L'1')
      ListAllVariables();
    else if (Key.UnicodeChar == L'2')
      SearchVariableByName();
    else if (Key.UnicodeChar == L'3')
      SearchVariableByGuid();
    else if (Key.UnicodeChar == L'4')
      CreateNewVariable();
    else if (Key.UnicodeChar == L'5')
      DeleteVariable();
    else if (Key.UnicodeChar == L'6') {
      Print(L"Exiting...\n");
      return EFI_SUCCESS;
    } else {
      Print(L"Invalid choice.\n");
    }

    Print(L"\nPress any key to continue...");
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  }
}

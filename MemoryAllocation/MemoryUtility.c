/** @file
This sample application bases on Memory setting 
to show how firmware allocates physical memory before OS boots.
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/ShellLib.h>

/**
  Memory allocation type enumeration
**/
typedef enum {
  AllocationTypeNone,
  AllocationTypePages,
  AllocationTypePool
} ALLOCATION_TYPE;

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in]  Argc  The number of parameters.
  @param[in]  Argv  The array of pointers to parameters.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR16 *UserInput = NULL;
  CHAR16 *AllocInput = NULL;
  UINTN MenuChoice = 0;
  UINTN AllocType = 0;
  VOID *AllocatedPool = NULL;
  EFI_PHYSICAL_ADDRESS AllocatedPages = 0;
  UINTN Pages = 1; // 1 page = 4KB
  UINTN PoolSize = 1; // 1 bytes
  ALLOCATION_TYPE CurrentAllocation = AllocationTypeNone;
  
  //
  // Mid-block Variables
  //
  VOID *TargetBuffer;
  UINTN BufferSize;
  CHAR16 TestString[] = L"UEFI Memory Test Data";
  UINTN CopySize;
  UINTN i;
  
  //
  // Memory map variables
  //
  UINTN MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR *MemoryMap;
  UINTN MapKey;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;
  UINTN TotalMemory;
  UINTN TotalPages;
  EFI_MEMORY_DESCRIPTOR *Desc;
  UINTN NumEntries;
  CHAR16 *TypeStr;

  //
  // Main application loop
  //
  while (1) {
    // Clear screen and display menu
    Print(L"\n=== UEFI Memory Utility ===\n\n");
    Print(L"1. Allocate Memory\n");
    Print(L"2. Write Data to Allocated Memory\n");
    Print(L"3. Free Memory\n");
    Print(L"4. Dump Memory Map\n");
    Print(L"5. Exit\n\n");
    Print(L"Current Status: ");
    
    switch (CurrentAllocation) {
      case AllocationTypeNone:
        Print(L"No memory allocated\n\n");
        break;
      case AllocationTypePages:
        Print(L"Pages allocated at 0x%lx (%d pages = %d KB)\n\n", 
              AllocatedPages, Pages, Pages * 4);
        break;
      case AllocationTypePool:
        Print(L"Pool allocated at 0x%lx (%d bytes)\n\n", 
              (UINTN)AllocatedPool, PoolSize);
        break;
    }

    //
    // Get user input
    //
    Print(L"Select option (1-5): ");    
    Status = ShellPromptForResponse(ShellPromptResponseTypeFreeform,
                                    NULL, 
                                    &UserInput);
    
    if (EFI_ERROR(Status) || UserInput == NULL) {
      Print(L"Error getting input! Status: %r\n", Status);
      continue;
    }

    //
    // Convert input to number
    //
    MenuChoice = StrDecimalToUintn(UserInput);
    FreePool(UserInput);
    UserInput = NULL;

    switch (MenuChoice) {
      case 1: // Allocate Memory
        Print(L"\nAllocation Type:\n");
        Print(L"1. Allocate Pages (%d pages = %d KB)\n", Pages, Pages * 4);
        Print(L"2. Allocate Pool (%d bytes)\n", PoolSize);
        Print(L"Select (1-2): ");
                
        Status = ShellPromptForResponse(ShellPromptResponseTypeFreeform, 
                                       NULL, 
                                       &AllocInput);
        
        if (EFI_ERROR(Status) || AllocInput == NULL) {
          Print(L"Error getting allocation type input: %r\n", Status);
          break;
        }

        AllocType = StrDecimalToUintn(AllocInput);
        FreePool(AllocInput);
        AllocInput = NULL;

        //
        // Free existing allocation before new allocation
        //
        if (CurrentAllocation != AllocationTypeNone) {
          Print(L"Freeing existing allocation first...\n");
          if (CurrentAllocation == AllocationTypePages) {
            gBS->FreePages(AllocatedPages, Pages);
          } else {
            gBS->FreePool(AllocatedPool);
          }
          CurrentAllocation = AllocationTypeNone;
        }

        if (AllocType == 1) {
          //
          // Allocate Pages
          //
          Status = gBS->AllocatePages(AllocateAnyPages, 
                                     EfiBootServicesData, 
                                     Pages, 
                                     &AllocatedPages);
          if (!EFI_ERROR(Status)) {
            CurrentAllocation = AllocationTypePages;
            Print(L"Successfully allocated %d pages at 0x%lx\n", 
                  Pages, AllocatedPages);
          } else {
            Print(L"Failed to allocate pages: %r\n", Status);
          }
        } else if (AllocType == 2) {
          //
          // Allocate Pool
          //
          Status = gBS->AllocatePool(EfiBootServicesData, 
                                    PoolSize, 
                                    &AllocatedPool);
          if (!EFI_ERROR(Status)) {
            CurrentAllocation = AllocationTypePool;
            Print(L"Successfully allocated pool at 0x%lx\n", (UINTN)AllocatedPool);
          } else {
            Print(L"Failed to allocate pool: %r\n", Status);
          }
        } else {
          Print(L"Invalid allocation type! Please enter 1 or 2.\n");
        }
        break;

      case 2: // Write Data to Memory
        if (CurrentAllocation == AllocationTypeNone) {
          Print(L"No memory allocated! Please allocate memory first.\n");
        } else {
          if (CurrentAllocation == AllocationTypePages) {
            TargetBuffer = (VOID *)(UINTN)AllocatedPages;
            BufferSize = Pages * EFI_PAGE_SIZE;
          } else {
            TargetBuffer = AllocatedPool;
            BufferSize = PoolSize;
          }
          
          //
          // Write test pattern to memory
          //
          SetMem(TargetBuffer, BufferSize, 0xAB); // Fill with 0xAB pattern
          
          //
          // Copy a string to demonstrate CopyMem
          //
          CopySize = StrSize(TestString);
          if (CopySize > BufferSize) {
            CopySize = BufferSize;
          }
          CopyMem(TargetBuffer, TestString, CopySize);
          
          Print(L"Successfully wrote data to allocated memory\n");
          
          //
          // Display first few bytes
          //
          Print(L"First 32 bytes (hex): ");
          for (i = 0; i < 32 && i < BufferSize; i++) {
            Print(L"%02x ", ((UINT8 *)TargetBuffer)[i]);
          }
          Print(L"\n");
        }
        break;

      case 3: // Free Memory
        if (CurrentAllocation == AllocationTypeNone) {
          Print(L"No memory allocated!\n");
        } else {
          if (CurrentAllocation == AllocationTypePages) {
            Status = gBS->FreePages(AllocatedPages, Pages);
            if (!EFI_ERROR(Status)) {
              Print(L"Successfully freed pages\n");
              CurrentAllocation = AllocationTypeNone;
            } else {
              Print(L"Failed to free pages: %r\n", Status);
            }
          } else {
            Status = gBS->FreePool(AllocatedPool);
            if (!EFI_ERROR(Status)) {
              Print(L"Successfully freed pool\n");
              CurrentAllocation = AllocationTypeNone;
              AllocatedPool = NULL;
            } else {
              Print(L"Failed to free pool: %r\n", Status);
            }
          }
        }
        break;

      case 4: // Dump Memory Map
        MemoryMapSize = 0;
        MemoryMap = NULL;
        //
        // First call to get required size
        //
        Status = gBS->GetMemoryMap(&MemoryMapSize, 
                                  MemoryMap, 
                                  &MapKey, 
                                  &DescriptorSize, 
                                  &DescriptorVersion);
        
        if (Status == EFI_BUFFER_TOO_SMALL) {
          //
          // Re-allocate buffer for memory map if the original one is too small
          //
          MemoryMapSize += 2 * DescriptorSize;
          Status = gBS->AllocatePool(EfiBootServicesData, 
                                    MemoryMapSize, 
                                    (VOID **)&MemoryMap);
          
          if (!EFI_ERROR(Status)) {
            //
            // Get the actual memory map
            //
            Status = gBS->GetMemoryMap(&MemoryMapSize, 
                                      MemoryMap, 
                                      &MapKey, 
                                      &DescriptorSize, 
                                      &DescriptorVersion);
            
            if (!EFI_ERROR(Status)) {
              TotalMemory = 0;
              TotalPages = 0;
              Desc = MemoryMap;
              NumEntries = MemoryMapSize / DescriptorSize;
              
              Print(L"\nMemory Map (%d entries):\n", NumEntries);
              Print(L"Type             Start        End          Attributes\n");
              Print(L"--------------   ----------   ----------   --------\n");
              
              for (i = 0; i < NumEntries; i++) {

                EFI_PHYSICAL_ADDRESS PhysicalEnd;
                PhysicalEnd = Desc->PhysicalStart + (Desc->NumberOfPages * EFI_PAGE_SIZE) - 1;

                //
                // Convert memory type to string
                //
                switch (Desc->Type) {
                  case EfiReservedMemoryType:       TypeStr = L"Reserved"; break;
                  case EfiLoaderCode:               TypeStr = L"LoaderCode"; break;
                  case EfiLoaderData:               TypeStr = L"LoaderData"; break;
                  case EfiBootServicesCode:         TypeStr = L"BS Code"; break;
                  case EfiBootServicesData:         TypeStr = L"BS Data"; break;
                  case EfiRuntimeServicesCode:      TypeStr = L"RT Code"; break;
                  case EfiRuntimeServicesData:      TypeStr = L"RT Data"; break;
                  case EfiConventionalMemory:       TypeStr = L"Conventional"; break;
                  case EfiUnusableMemory:           TypeStr = L"Unusable"; break;
                  case EfiACPIReclaimMemory:        TypeStr = L"ACPI Reclaim"; break;
                  case EfiACPIMemoryNVS:            TypeStr = L"ACPI NVS"; break;
                  case EfiMemoryMappedIO:           TypeStr = L"MMIO"; break;
                  case EfiMemoryMappedIOPortSpace:  TypeStr = L"MMIO Port"; break;
                  case EfiPalCode:                  TypeStr = L"PAL Code"; break;
                  default:                          TypeStr = L"Unknown"; break;
                }
                
                Print(L"%-14s   0x%08x   0x%08x   %08lx\n", 
                      TypeStr, 
                      Desc->PhysicalStart,
                      PhysicalEnd, 
                      Desc->Attribute);
                
                TotalPages += Desc->NumberOfPages;
                Desc = NEXT_MEMORY_DESCRIPTOR(Desc, DescriptorSize);
              }
              
              TotalMemory = TotalPages * 4; // Convert pages to KB
              Print(L"\nTotal Memory: %d KB (%d pages)\n", TotalMemory, TotalPages);

            } else {
              Print(L"Failed to get memory map on second call: %r\n", Status);
            }
            
            gBS->FreePool(MemoryMap);
          } else {
            Print(L"Failed to allocate memory for memory map: %r\n", Status);
          }
        } else {
          Print(L"Failed to get memory map size: %r\n", Status);
        }
        Print(L"\nPress any key to continue...");
        ShellPromptForResponse(ShellPromptResponseTypeAnyKeyContinue, NULL, NULL);
        break;

      case 5: // Exit
        //
        // Free memory before exiting if any is allocated
        //
        if (CurrentAllocation != AllocationTypeNone) {
          Print(L"Freeing allocated memory before exit...\n");
          if (CurrentAllocation == AllocationTypePages) {
            gBS->FreePages(AllocatedPages, Pages);
          } else {
            gBS->FreePool(AllocatedPool);
          }
        }
        Print(L"Exiting UEFI Memory Utility...\n");
        return EFI_SUCCESS;

      default:
        Print(L"Invalid option! Please select 1-5.\n");
        break;
    }
    
    Print(L"\nPress any key to continue...\n");
    ShellPromptForResponse(ShellPromptResponseTypeAnyKeyContinue, NULL, NULL);
  }

  return EFI_SUCCESS;
}
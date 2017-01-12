/** @file
*
*  Copyright (c) 2011, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/ArmLib.h>
#include <Library/PrePiLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <LittleKernel.h>

STATIC LIST_ENTRY mMappings;

#define RangesOverlap(x1, x2, y1, y2) (MAX((x1), (y1)) <= MIN((x2), (y2)))
#define InsertAfterList(entry, new_entry) InsertHeadList(entry, new_entry)
#define InsertBeforeList(entry, new_entry) InsertTailList(entry, new_entry)

#define ListForEvery(List, Link) \
  for(Link = (List)->ForwardLink; Link != (List); Link = Link->ForwardLink)

#define ListForEveryEntry(List, Link, entry, type, member, signature) \
  for(Link = (List)->ForwardLink;\
      Link != (List) && ((entry) = CR((Link), type, member, signature));\
      Link = Link->ForwardLink)

#define PeekHeadTypeList(list, type, element, signature) ({\
  LIST_ENTRY *__nod = PeekHeadList(list);\
  type *__t;\
  if(__nod)\
    __t = CR(__nod, type, element, signature);\
  else\
    __t = (type *)0;\
  __t;\
})

#define PeekTailTypeList(list, type, element, signature) ({\
  LIST_ENTRY *__nod = PeekTailList(list);\
  type *__t;\
  if(__nod)\
    __t = CR(__nod, type, element, signature);\
  else\
    __t = (type *)0;\
  __t;\
})

STATIC
inline
LIST_ENTRY *
PeekHeadList (
  LIST_ENTRY *List
)
{
  if (List->ForwardLink != List) {
    return List->ForwardLink;
  } else {
    return NULL;
  }
}

STATIC
inline
LIST_ENTRY *
PeekTailList (
  LIST_ENTRY *List
)
{
  if (List->BackLink != List) {
    return List->BackLink;
  } else {
    return NULL;
  }
}

STATIC
inline
UINTN
ListLength (
  LIST_ENTRY *List
)
{
  UINTN Count = 0;
  LIST_ENTRY *Link = List;
  ListForEvery(List, Link) {
    Count++;
  }

  return Count;
}

#define MMAP_LIST_SIGNATURE             SIGNATURE_32 ('m', 'm', 'a', 'p')
typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;
  LIST_ENTRY      TmpLink;

  lkapi_mmap_rangeflags_t RangeFlags;
  UINT64                  Start;
  UINT64                  End;

  // VirtualMemoryTable
  // use LKAPI_MEMORYATTR_DONT_MAP to disable MMU mappings
  ARM_MEMORY_REGION_ATTRIBUTES MemoryAttributes;

  // BuildMemoryAllocationHob (only for system memory)
  // only used if LKAPI_MMAP_RANGEFLAG_RESERVED|LKAPI_MMAP_RANGEFLAG_DRAM is set
  EFI_MEMORY_TYPE              MemoryType;
} MMAP_RANGE;

VOID
MmapPrintMappings (
  LIST_ENTRY *Mappings
);

STATIC
MMAP_RANGE *
MmapDupRange (
  MMAP_RANGE* Base
)
{
  MMAP_RANGE *Item = AllocatePool(sizeof(MMAP_RANGE));
  if (!Item) return NULL;

  CopyMem(Item, Base, sizeof(*Item));
  return Item;
}

EFI_STATUS
MmapInsertRange (
  LIST_ENTRY              *Mappings,
  MMAP_RANGE              *NewItem,
  lkapi_mmap_rangeflags_t RangeFlags
)
{
  LIST_ENTRY CombinationList;
  InitializeListHead(&CombinationList);

  MMAP_RANGE *Item;
  LIST_ENTRY *Link;
  ListForEveryEntry(Mappings, Link, Item, MMAP_RANGE, Link, MMAP_LIST_SIGNATURE) {
    // never allow using reserved memory
    if (Item->RangeFlags&LKAPI_MMAP_RANGEFLAG_RESERVED)
      continue;
    // use requested range types only
    if (!(Item->RangeFlags&RangeFlags))
      continue;

    if (Item->Start<=NewItem->Start && Item->End>=NewItem->End) {
      BOOLEAN IsAtStart = NewItem->Start==Item->Start;
      BOOLEAN IsAtEnd = NewItem->End==Item->End;
      BOOLEAN IsSame = IsAtStart && IsAtEnd;

      if (IsSame) {
        InsertAfterList(&Item->Link, &NewItem->Link);
        RemoveEntryList(&Item->Link);
        FreePool(Item);
      }

      else if(IsAtStart && !IsAtEnd) {
        Item->Start = NewItem->End+1;
        InsertBeforeList(&Item->Link, &NewItem->Link);
      }

      else if(!IsAtStart && IsAtEnd) {
        Item->End = NewItem->Start-1;
        InsertAfterList(&Item->Link, &NewItem->Link);
      }

      else {
        MMAP_RANGE* Right = MmapDupRange(Item);

        Item->End = NewItem->Start-1;
        Right->Start = NewItem->End+1;
        InsertAfterList(&Item->Link, &NewItem->Link);
        InsertAfterList(&NewItem->Link, &Right->Link);
      }
      return EFI_SUCCESS;
    }

    else if(RangesOverlap(Item->Start, Item->End, NewItem->Start, NewItem->End)) {
      InsertTailList(&CombinationList, &Item->TmpLink);
    }
  }

  if (!IsListEmpty(&CombinationList)) {
    // check if items are coherent
    ListForEveryEntry(&CombinationList, Link, Item, MMAP_RANGE, TmpLink, MMAP_LIST_SIGNATURE) {
      MMAP_RANGE *Next = CR(Item->TmpLink.ForwardLink, MMAP_RANGE, TmpLink, MMAP_LIST_SIGNATURE);

      if(Item->TmpLink.ForwardLink!=&CombinationList && (Next->Start!=Item->End+1)) {
        DEBUG((EFI_D_ERROR, "not coherent: 0x%016llx - 0x%016llx\n", Item->Start, Item->End));
        return EFI_NOT_FOUND;
      }
    }

    // check if the total range covers the whole range of NewItem
    MMAP_RANGE *Head = PeekHeadTypeList(&CombinationList, MMAP_RANGE, TmpLink, MMAP_LIST_SIGNATURE);
    MMAP_RANGE *Tail = PeekTailTypeList(&CombinationList, MMAP_RANGE, TmpLink, MMAP_LIST_SIGNATURE);
    if (!(NewItem->Start>=Head->Start && NewItem->End<=Tail->End)) {
      DEBUG((EFI_D_ERROR, "range isn't big enough: 0x%016llx - 0x%016llx\n", Head->Start, Tail->End));
      return EFI_NOT_FOUND;
    }

    // delete Items in the middle
    ListForEveryEntry(&CombinationList, Link, Item, MMAP_RANGE, TmpLink, MMAP_LIST_SIGNATURE) {
      if (Item!=Head && Item!=Tail)
        RemoveEntryList(&Item->Link);
    }

    // put new item after the head
    InsertAfterList(&Head->Link, &NewItem->Link);

    // delete or trim head
    if (NewItem->Start==Head->Start) {
      RemoveEntryList(&Head->Link);
      FreePool(Head);
    }
    else {
      Head->End = NewItem->Start-1;
    }

    // delete or trim tail
    if (NewItem->End==Tail->End) {
      RemoveEntryList(&Tail->Link);
      FreePool(Tail);
    }
    else {
      Tail->Start = NewItem->End+1;
    }

    return EFI_SUCCESS;
  }

  DEBUG((EFI_D_ERROR, "can't find spot for 0x%016llx - 0x%016llx\n", NewItem->Start, NewItem->End));
  return EFI_NOT_FOUND;
}

VOID
MmapPrintMappings (
  LIST_ENTRY *Mappings
)
{
  DEBUG((EFI_D_ERROR, "\nMAPPINGS\n"));

  MMAP_RANGE *Item;
  LIST_ENTRY *Link;
  ListForEveryEntry(Mappings, Link, Item, MMAP_RANGE, Link, MMAP_LIST_SIGNATURE) {
    DEBUG((EFI_D_ERROR, "0x%016llx - 0x%016llx: rangeflags=%d attributes=%d type=%d\n", Item->Start, Item->End, Item->RangeFlags, 
           Item->MemoryAttributes, Item->MemoryType));
  }

  DEBUG((EFI_D_ERROR, "\n"));
}

UINTN
LKApiMemoryAttr2Efi (
  long MemoryAttribute
)
{
  switch(MemoryAttribute) {
    case LKAPI_MEMORYATTR_DONT_MAP:
      return (UINTN)-1;
    case LKAPI_MEMORYATTR_UNCACHED:
      return ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED;
    case LKAPI_MEMORYATTR_WRITE_BACK:
      return ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK;
    case LKAPI_MEMORYATTR_WRITE_THROUGH:
      return ARM_MEMORY_REGION_ATTRIBUTE_WRITE_THROUGH;
    case LKAPI_MEMORYATTR_DEVICE:
      return ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;
    default:
      ASSERT(FALSE);
      return 0;
  }
}

EFI_MEMORY_TYPE
LKApiMemoryType2Efi (
  long MemoryType
)
{
  switch(MemoryType) {
    case LKAPI_MEMORYTYPE_RESERVED:
      return EfiReservedMemoryType;
    case LKAPI_MEMORYTYPE_LOADER_CODE:
      return EfiLoaderCode;
    case LKAPI_MEMORYTYPE_LOADER_DATA:
      return EfiLoaderData;
    case LKAPI_MEMORYTYPE_BOOTSERVICES_CODE:
      return EfiBootServicesCode;
    case LKAPI_MEMORYTYPE_BOOTSERVICES_DATA:
      return EfiBootServicesData;
    case LKAPI_MEMORYTYPE_RUNTIMESERVICES_CODE:
      return EfiRuntimeServicesCode;
    case LKAPI_MEMORYTYPE_RUNTIMESERVICES_DATA:
      return EfiRuntimeServicesData;
    case LKAPI_MEMORYTYPE_CONVENTIONAL_MEMORY:
      return EfiConventionalMemory;
    case LKAPI_MEMORYTYPE_UNUSABLE_MEMORY:
      return EfiUnusableMemory;
    case LKAPI_MEMORYTYPE_MEMORY_MAPPED_IO:
      return EfiMemoryMappedIO;
    case LKAPI_MEMORYTYPE_MEMORY_MAPPED_IO_PORTSPACE:
      return EfiMemoryMappedIOPortSpace;
    case LKAPI_MEMORYTYPE_PAL_CODE:
      return EfiPalCode;
    case LKAPI_MEMORYTYPE_PERSISTENT:
      return EfiPersistentMemory;
    default:
      ASSERT(FALSE);
      return 0;
  }
}

VOID *
LKApiMmapAdd (
  VOID                          *PData,
  unsigned long long            Start,
  unsigned long long            Size,
  lkapi_mmap_rangeflags_t       RangeFlags,
  long                          MemoryAttribute,
  long                          MemoryType,
  lkapi_mmap_rangeflags_t       InsertInRangeFlags
)
{
  LIST_ENTRY *Mappings = PData;

  MMAP_RANGE *Range = AllocatePool(sizeof(MMAP_RANGE));
  ASSERT(Range);

  Range->Signature        = MMAP_LIST_SIGNATURE;
  Range->RangeFlags       = RangeFlags;
  Range->Start            = Start;
  Range->End              = Start + (Size-1);
  Range->MemoryAttributes = LKApiMemoryAttr2Efi(MemoryAttribute);
  Range->MemoryType       = LKApiMemoryType2Efi(MemoryType);

  MmapInsertRange (Mappings, Range, InsertInRangeFlags);
  return PData;
}

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR describing a Physical-to-
                                    Virtual Memory mapping. This array must be ended by a zero-filled
                                    entry

**/
VOID
ArmPlatformGetVirtualMemoryMap (
  IN ARM_MEMORY_REGION_DESCRIPTOR** VirtualMemoryMap
  )
{
  UINTN                         Index = 0;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable;
  lkapi_t* LKApi = GetLKApi();

  ASSERT(VirtualMemoryMap != NULL);

  // initialize internal memory map
  InitializeListHead(&mMappings);
  MMAP_RANGE *Range = AllocatePool(sizeof(MMAP_RANGE));
  ASSERT(Range);
  Range->Signature        = MMAP_LIST_SIGNATURE;
  Range->RangeFlags       = LKAPI_MMAP_RANGEFLAG_UNUSED;
  Range->Start            = 0x0;
  Range->End              = (UINTN)-1;
  if (LKApi->mmap_needs_identity_map())
    Range->MemoryAttributes = LKApiMemoryAttr2Efi(LKAPI_MEMORYATTR_DEVICE);
  else
    Range->MemoryAttributes = LKApiMemoryAttr2Efi(LKAPI_MEMORYATTR_DONT_MAP);
  Range->MemoryType       = 0;
  InsertTailList(&mMappings, &Range->Link);

  // add mappings from LKApi
  LKApi->mmap_get_all(&mMappings, LKApiMmapAdd);

  // add system memory as separate memory, but don't create a Hob for it, because MemoryInitPei does that for us
  LKApiMmapAdd(&mMappings, PcdGet64 (PcdSystemMemoryBase), PcdGet64 (PcdSystemMemorySize), LKAPI_MMAP_RANGEFLAG_RESERVED,
               LKAPI_MEMORYATTR_WRITE_BACK, 0, LKAPI_MMAP_RANGEFLAG_DRAM);

  MmapPrintMappings(&mMappings);

  // allocate MMU table
  VirtualMemoryTable = AllocatePages(EFI_SIZE_TO_PAGES (sizeof(ARM_MEMORY_REGION_DESCRIPTOR) * ListLength(&mMappings)));
  if (VirtualMemoryTable == NULL) {
    return;
  }

  // build MMU table
  MMAP_RANGE *Item;
  LIST_ENTRY *Link;
  ListForEveryEntry(&mMappings, Link, Item, MMAP_RANGE, Link, MMAP_LIST_SIGNATURE) {
    // LKAPI_MEMORYATTR_DONT_MAP
    if (Item->MemoryAttributes==(UINTN)-1)
      continue;

    VirtualMemoryTable[Index].PhysicalBase = Item->Start;
    VirtualMemoryTable[Index].VirtualBase  = Item->Start;
    VirtualMemoryTable[Index].Length       = (Item->End - Item->Start) + 1;
    VirtualMemoryTable[Index].Attributes   = Item->MemoryAttributes;

    Index++;
  }

  // End of Table
  VirtualMemoryTable[Index].PhysicalBase = 0;
  VirtualMemoryTable[Index].VirtualBase  = 0;
  VirtualMemoryTable[Index].Length       = 0;
  VirtualMemoryTable[Index].Attributes   = (ARM_MEMORY_REGION_ATTRIBUTES)0;

  // build DRAM Hob's
  ListForEveryEntry(&mMappings, Link, Item, MMAP_RANGE, Link, MMAP_LIST_SIGNATURE) {
    EFI_RESOURCE_ATTRIBUTE_TYPE   ResourceAttributes;
    if (!(Item->RangeFlags&LKAPI_MMAP_RANGEFLAG_DRAM))
      continue;

    ResourceAttributes = (
        EFI_RESOURCE_ATTRIBUTE_PRESENT |
        EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
        EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
        EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
        EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
        EFI_RESOURCE_ATTRIBUTE_TESTED
    );

    BuildResourceDescriptorHob (
      EFI_RESOURCE_SYSTEM_MEMORY,
      ResourceAttributes,
      Item->Start,
      (Item->End - Item->Start) + 1
    );
  }

  // return MMU table
  *VirtualMemoryMap = VirtualMemoryTable;
}

VOID
ArmPlatformBuildMemoryAllocationHobs (
  VOID
  )
{
  MMAP_RANGE *Item;
  LIST_ENTRY *Link;
  ListForEveryEntry(&mMappings, Link, Item, MMAP_RANGE, Link, MMAP_LIST_SIGNATURE) {
    if (!(Item->RangeFlags&LKAPI_MMAP_RANGEFLAG_DRAM))
      continue;
    if (!(Item->RangeFlags&LKAPI_MMAP_RANGEFLAG_RESERVED))
      continue;

    BuildMemoryAllocationHob (Item->Start, (Item->End - Item->Start) + 1, Item->MemoryType);
  }
}

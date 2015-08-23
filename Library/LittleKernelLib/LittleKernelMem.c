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
#include <LittleKernel.h>

// DDR attributes
#define DDR_ATTRIBUTES_CACHED                ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK
#define DDR_ATTRIBUTES_UNCACHED              ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED

#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS          3

STATIC UINT32 mDramCount = 0;
STATIC UINT32 mIomapCount = 0;

static void* mmap_callback_dram_buildhob(void* pdata, unsigned long addr, unsigned long size, int reserved) {
  EFI_RESOURCE_ATTRIBUTE_TYPE   ResourceAttributes;

  if(!reserved) {
    ResourceAttributes = (
        EFI_RESOURCE_ATTRIBUTE_PRESENT |
        EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
        EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
        EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
        EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
        EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
        EFI_RESOURCE_ATTRIBUTE_TESTED
    );

    BuildResourceDescriptorHob (
      EFI_RESOURCE_SYSTEM_MEMORY,
      ResourceAttributes,
      addr,
      size
    );

    mDramCount++;
  }

  return pdata;
}

static void* mmap_callback_dram_addmap(void* pdata, unsigned long addr, unsigned long size, int reserved) {
  ARM_MEMORY_REGION_ATTRIBUTES  CacheAttributes;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)pdata;

  if(!reserved) {
    if (FeaturePcdGet(PcdCacheEnable) == TRUE) {
      CacheAttributes = DDR_ATTRIBUTES_CACHED;
    } else {
      CacheAttributes = DDR_ATTRIBUTES_UNCACHED;
    }

    VirtualMemoryTable->PhysicalBase = addr;
    VirtualMemoryTable->VirtualBase  = addr;
    VirtualMemoryTable->Length       = size;
    VirtualMemoryTable->Attributes   = CacheAttributes;

    return (void*)(VirtualMemoryTable+1);
  }

  return pdata;
}

static void* mmap_callback_iomap_count(void* pdata, unsigned long addr, unsigned long size, int reserved) {
  mIomapCount++;
  return pdata;
}

static void* mmap_callback_iomap_addmap(void* pdata, unsigned long addr, unsigned long size, int reserved) {
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)pdata;

  VirtualMemoryTable->PhysicalBase = addr;
  VirtualMemoryTable->VirtualBase  = addr;
  VirtualMemoryTable->Length       = size;
  VirtualMemoryTable->Attributes   = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  return (void*)(VirtualMemoryTable+1);
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
  UINTN                         NumTableEntries = 0;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTablePtr;
  lkapi_t* LKApi = GetLKApi();

  ASSERT(VirtualMemoryMap != NULL);

  // build memory resource Hob's
  LKApi->mmap_get_dram(NULL, mmap_callback_dram_buildhob);

  // count IOMAP entries
  LKApi->mmap_get_iomap(NULL, mmap_callback_iomap_count);

  NumTableEntries = (mDramCount + mIomapCount + 1);
  VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)AllocatePages(EFI_SIZE_TO_PAGES (sizeof(ARM_MEMORY_REGION_DESCRIPTOR) * NumTableEntries));
  if (VirtualMemoryTable == NULL) {
    return;
  }

  // add IOMAP entries
  VirtualMemoryTablePtr = LKApi->mmap_get_iomap(VirtualMemoryTable, mmap_callback_iomap_addmap);
  Index+=mIomapCount;

  // add DRAM entries
  VirtualMemoryTablePtr = LKApi->mmap_get_dram(VirtualMemoryTablePtr, mmap_callback_dram_addmap);
  Index+=mDramCount;

  // End of Table
  VirtualMemoryTablePtr->PhysicalBase = 0;
  VirtualMemoryTablePtr->VirtualBase  = 0;
  VirtualMemoryTablePtr->Length       = 0;
  VirtualMemoryTablePtr->Attributes   = (ARM_MEMORY_REGION_ATTRIBUTES)0;
  VirtualMemoryTablePtr++;

  ASSERT(VirtualMemoryTablePtr==(VirtualMemoryTable+NumTableEntries));

  *VirtualMemoryMap = VirtualMemoryTable;
}

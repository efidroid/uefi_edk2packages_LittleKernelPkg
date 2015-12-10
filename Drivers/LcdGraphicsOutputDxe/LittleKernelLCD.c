/** @file  Lcd.c

  Copyright (c) 2011-2012, ARM Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/LcdPlatformLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/Cpu.h>

#include "LcdGraphicsOutputDxe.h"

EFI_STATUS
LcdInitialize (
  VOID
  )
{
  return LKApi->lcd_init()==0?EFI_SUCCESS:EFI_DEVICE_ERROR;
}

EFI_STATUS
LcdSetMode (
  IN UINT32  ModeNumber
  )
{

  return EFI_SUCCESS;
}

VOID
LcdShutdown (
  VOID
  )
{
  LKApi->lcd_shutdown();
}

typedef struct {
  UINT32                     HorizontalResolution;
  UINT32                     VerticalResolution;
  lkapi_lcd_pixelformat_t    PixelFormat;
} LCD_RESOLUTION;


STATIC LCD_RESOLUTION mResolutions[] = {
  {
    0, 0, LKAPI_LCD_PIXELFORMAT_INVALID
  },
  {
    0, 0, LKAPI_LCD_PIXELFORMAT_INVALID
  }
};

EFI_STATUS
LcdPlatformInitializeDisplay (
  IN EFI_HANDLE   Handle
  )
{
  // native orientation
  mResolutions[0].HorizontalResolution = LKApi->lcd_get_width();
  mResolutions[0].VerticalResolution   = LKApi->lcd_get_height();
  mResolutions[0].PixelFormat          = LKApi->lcd_get_pixelformat();

  // rotated 90deg clockwise
  mResolutions[1].HorizontalResolution = LKApi->lcd_get_height();
  mResolutions[1].VerticalResolution   = LKApi->lcd_get_width();
  mResolutions[1].PixelFormat          = LKApi->lcd_get_pixelformat();

  return EFI_SUCCESS;
}

EFI_STATUS
LcdPlatformGetVram (
  OUT EFI_PHYSICAL_ADDRESS*  VramBaseAddress,
  OUT UINTN*                 VramSize
  )
{
  EFI_STATUS              Status;
  EFI_CPU_ARCH_PROTOCOL  *Cpu;
  EFI_ALLOCATE_TYPE       AllocationType;

  // Set the vram size
  *VramSize = LCD_VRAM_SIZE;

  *VramBaseAddress = (EFI_PHYSICAL_ADDRESS)LKApi->lcd_get_vram_address();

  // Allocate the VRAM from the DRAM so that nobody else uses it.
  if (*VramBaseAddress == 0) {
    AllocationType = AllocateAnyPages;
  } else {
    AllocationType = AllocateAddress;
  }
  Status = gBS->AllocatePages (AllocationType, EfiBootServicesData, EFI_SIZE_TO_PAGES(((UINTN)LCD_VRAM_SIZE)), VramBaseAddress);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // Ensure the Cpu architectural protocol is already installed
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&Cpu);
  ASSERT_EFI_ERROR(Status);

  // Mark the VRAM as un-cacheable. The VRAM is inside the DRAM, which is cacheable.
  Status = Cpu->SetMemoryAttributes (Cpu, *VramBaseAddress, *VramSize, EFI_MEMORY_UC);
  ASSERT_EFI_ERROR(Status);
  if (EFI_ERROR(Status)) {
    gBS->FreePool (VramBaseAddress);
    return Status;
  }

  return EFI_SUCCESS;
}

UINT32
LcdPlatformGetMaxMode (
  VOID
  )
{
  //
  // The following line will report correctly the total number of graphics modes
  // that could be supported by the graphics driver:
  //
  return (sizeof(mResolutions) / sizeof(LCD_RESOLUTION));
}

EFI_STATUS
LcdPlatformSetMode (
  IN UINT32                         ModeNumber
  )
{
  EFI_STATUS            Status;

  if (ModeNumber >= LcdPlatformGetMaxMode ()) {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;

  return Status;
}

EFI_STATUS
LcdPlatformQueryMode (
  IN  UINT32                                ModeNumber,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info
  )
{
  if (ModeNumber >= LcdPlatformGetMaxMode ()) {
    return EFI_INVALID_PARAMETER;
  }

  Info->Version = 0;
  Info->HorizontalResolution = mResolutions[ModeNumber].HorizontalResolution;
  Info->VerticalResolution = mResolutions[ModeNumber].VerticalResolution;
  Info->PixelsPerScanLine = mResolutions[ModeNumber].HorizontalResolution;

  // we use a sw buffer in EFI's native format
  Info->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;

  return EFI_SUCCESS;
}

UINT32
LKDisplayGetPortraitMode (
  VOID
)
{
  if (mResolutions[0].HorizontalResolution > mResolutions[0].VerticalResolution)
    return 1;
  else
    return 0;
}

UINT32
LKDisplayGetLandscapeMode (
  VOID
)
{
  if (mResolutions[0].HorizontalResolution > mResolutions[0].VerticalResolution)
    return 0;
  else
    return 1;
}

lkapi_lcd_pixelformat_t
LcdGetPixelFormat (
  VOID
  )
{
  return mResolutions[0].PixelFormat;
}

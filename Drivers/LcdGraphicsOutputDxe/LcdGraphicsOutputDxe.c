/** @file

 Copyright (c) 2011-2014, ARM Ltd. All rights reserved.<BR>
 This program and the accompanying materials
 are licensed and made available under the terms and conditions of the BSD License
 which accompanies this distribution.  The full text of the license may be found at
 http://opensource.org/licenses/bsd-license.php

 THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

 **/

#include <PiDxe.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Guid/GlobalVariable.h>

#include "LcdGraphicsOutputDxe.h"

#define MS2100N(x) ((x)*(1000000/100))
#define FPS2MS(x) (1000/(x))

/**********************************************************************
 *
 *  This file implements the Graphics Output protocol on ArmVersatileExpress
 *  using the Lcd controller
 *
 **********************************************************************/

//
// Global variables
//

BOOLEAN mDisplayInitialized = FALSE;
BOOLEAN gLcdNeedsSync = FALSE;
LK_DISPLAY_FLUSH_MODE gLCDFlushMode = LK_DISPLAY_FLUSH_MODE_AUTO;
lkapi_t* LKApi = NULL;
EFI_EVENT mTimerEvent;
STATIC UINT64 mLastFlush = 0;

LCD_INSTANCE mLcdTemplate = {
  LCD_INSTANCE_SIGNATURE,
  NULL, // Handle
  { // ModeInfo
    0, // Version
    0, // HorizontalResolution
    0, // VerticalResolution
    PixelBltOnly, // PixelFormat
    { 0 }, // PixelInformation
    0, // PixelsPerScanLine
  },
  {
    0, // MaxMode;
    0, // Mode;
    NULL, // Info;
    0, // SizeOfInfo;
    0, // FrameBufferBase;
    0 // FrameBufferSize;
  },
  { // Gop
    LcdGraphicsQueryMode,  // QueryMode
    LcdGraphicsSetMode,    // SetMode
    LcdGraphicsBlt,        // Blt
    NULL                     // *Mode
  },
  { // DevicePath
    {
      {
        HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
        { (UINT8) (sizeof(VENDOR_DEVICE_PATH)), (UINT8) ((sizeof(VENDOR_DEVICE_PATH)) >> 8) },
      },
      // Hardware Device Path for Lcd
      EFI_CALLER_ID_GUID // Use the driver's GUID
    },

    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      { sizeof(EFI_DEVICE_PATH_PROTOCOL), 0 }
    }
  },
  { // LKDisplay
    LKDisplayGetDensity,
    LKDisplaySetFlushMode,
    LKDisplayGetFlushMode,
    LKDisplayFlushScreen,
    LKDisplayGetPortraitMode,
    LKDisplayGetLandscapeMode
  },
  (EFI_EVENT) NULL // ExitBootServicesEvent
};

EFI_STATUS
LcdInstanceContructor (
  OUT LCD_INSTANCE** NewInstance
  )
{
  LCD_INSTANCE* Instance;

  Instance = AllocateCopyPool (sizeof(LCD_INSTANCE), &mLcdTemplate);
  if (Instance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Instance->Gop.Mode          = &Instance->Mode;
  Instance->Gop.Mode->MaxMode = LcdPlatformGetMaxMode ();
  Instance->Mode.Info         = &Instance->ModeInfo;

  *NewInstance = Instance;
  return EFI_SUCCESS;
}

//
// Function Definitions
//

EFI_STATUS
InitializeDisplay (
  IN LCD_INSTANCE* Instance
  )
{
  EFI_STATUS             Status = EFI_SUCCESS;
  EFI_PHYSICAL_ADDRESS   VramBaseAddress;
  UINTN                  VramSize;
  VOID                   *VramDoubleBuffer;

  // Setup the LCD
  Status = LcdInitialize ();
  if (EFI_ERROR(Status)) {
    goto EXIT_ERROR_LCD_SHUTDOWN;
  }

  // get VRAM address
  Status = LcdPlatformGetVram (&VramBaseAddress, &VramSize);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = LcdPlatformInitializeDisplay (Instance->Handle);
  if (EFI_ERROR(Status)) {
    goto EXIT_ERROR_LCD_SHUTDOWN;
  }

  VramDoubleBuffer = AllocatePool (VramSize);
  if (VramDoubleBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // Setup all the relevant mode information
  Instance->Gop.Mode->SizeOfInfo      = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  Instance->Gop.Mode->FrameBufferBase = (EFI_PHYSICAL_ADDRESS)(UINT32) VramDoubleBuffer;
  Instance->Gop.Mode->FrameBufferSize = VramSize;
  Instance->FrameBufferBase           = VramBaseAddress;

  // Set the flag before changing the mode, to avoid infinite loops
  mDisplayInitialized = TRUE;

  // All is ok, so don't deal with any errors
  goto EXIT;

EXIT_ERROR_LCD_SHUTDOWN:
  DEBUG((DEBUG_ERROR, "InitializeDisplay: ERROR - Can not initialise the display. Exit Status=%r\n", Status));
  LcdShutdown ();

EXIT:
  return Status;
}

VOID
EFIAPI
TimerCallback (
  IN  EFI_EVENT   Event,
  IN  VOID        *Context
  )
{
  LCD_INSTANCE* Instance = Context;

  if(gLcdNeedsSync && gLCDFlushMode==LK_DISPLAY_FLUSH_MODE_AUTO) {
    Instance->LKDisplay.FlushScreen(&Instance->LKDisplay);
  }
}

EFI_STATUS
EFIAPI
LcdGraphicsOutputDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  LCD_INSTANCE* Instance;

  LKApi = GetLKApi();

  Status = LcdInstanceContructor (&Instance);
  if (EFI_ERROR(Status)) {
    goto EXIT;
  }

  // Install the Graphics Output Protocol and the Device Path
  Status = gBS->InstallMultipleProtocolInterfaces(
            &Instance->Handle,
            &gEfiGraphicsOutputProtocolGuid, &Instance->Gop,
            &gEfiDevicePathProtocolGuid,     &Instance->DevicePath,
            &gEfiLKDisplayProtocolGuid,      &Instance->LKDisplay,
            NULL
            );

  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "GraphicsOutputDxeInitialize: Can not install the protocol. Exit Status=%r\n", Status));
    goto EXIT;
  }

  // Register for an ExitBootServicesEvent
  // When ExitBootServices starts, this function here will make sure that the graphics driver will shut down properly,
  // i.e. it will free up all allocated memory and perform any necessary hardware re-configuration.
  Status = gBS->CreateEvent (
            EVT_SIGNAL_EXIT_BOOT_SERVICES,
            TPL_NOTIFY,
            LcdGraphicsExitBootServicesEvent, NULL,
            &Instance->ExitBootServicesEvent
            );

  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "GraphicsOutputDxeInitialize: Can not install the ExitBootServicesEvent handler. Exit Status=%r\n", Status));
    goto EXIT_ERROR_UNINSTALL_PROTOCOL;
  }

  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, TimerCallback, Instance, &mTimerEvent);
  ASSERT_EFI_ERROR (Status);

  Status = gBS->SetTimer (mTimerEvent, TimerPeriodic, MS2100N(FPS2MS(30)));
  ASSERT_EFI_ERROR (Status);

  // To get here, everything must be fine, so just exit
  goto EXIT;

EXIT_ERROR_UNINSTALL_PROTOCOL:
  /* The following function could return an error message,
   * however, to get here something must have gone wrong already,
   * so preserve the original error, i.e. don't change
   * the Status variable, even it fails to uninstall the protocol.
   */
  gBS->UninstallMultipleProtocolInterfaces (
    Instance->Handle,
    &gEfiGraphicsOutputProtocolGuid, &Instance->Gop, // Uninstall Graphics Output protocol
    &gEfiDevicePathProtocolGuid,     &Instance->DevicePath,     // Uninstall device path
    &gEfiLKDisplayProtocolGuid,      &Instance->LKDisplay,
    NULL
    );

EXIT:
  return Status;

}

/***************************************
 * This function should be called
 * on Event: ExitBootServices
 * to free up memory, stop the driver
 * and uninstall the protocols
 ***************************************/
VOID
LcdGraphicsExitBootServicesEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  // By default, this PCD is FALSE. But if a platform starts a predefined OS that
  // does not use a framebuffer then we might want to disable the display controller
  // to avoid to display corrupted information on the screen.
  if (FeaturePcdGet (PcdGopDisableOnExitBootServices)) {
    // Turn-off the Display controller
    LcdShutdown ();
  }
}

/***************************************
 * GraphicsOutput Protocol function, mapping to
 * EFI_GRAPHICS_OUTPUT_PROTOCOL.QueryMode
 ***************************************/
EFI_STATUS
EFIAPI
LcdGraphicsQueryMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL            *This,
  IN UINT32                                  ModeNumber,
  OUT UINTN                                  *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION   **Info
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  LCD_INSTANCE *Instance;

  Instance = LCD_INSTANCE_FROM_GOP_THIS(This);

  // Setup the hardware if not already done
  if( !mDisplayInitialized ) {
    Status = InitializeDisplay(Instance);
    if (EFI_ERROR(Status)) {
      goto EXIT;
    }
  }

  // Error checking
  if ( (This == NULL) || (Info == NULL) || (SizeOfInfo == NULL) || (ModeNumber >= This->Mode->MaxMode) ) {
    DEBUG((DEBUG_ERROR, "LcdGraphicsQueryMode: ERROR - For mode number %d : Invalid Parameter.\n", ModeNumber ));
    Status = EFI_INVALID_PARAMETER;
    goto EXIT;
  }

  *Info = AllocatePool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  if (*Info == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  *SizeOfInfo = sizeof( EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  Status = LcdPlatformQueryMode (ModeNumber,*Info);
  if (EFI_ERROR(Status)) {
    FreePool(*Info);
  }

EXIT:
  return Status;
}

/***************************************
 * GraphicsOutput Protocol function, mapping to
 * EFI_GRAPHICS_OUTPUT_PROTOCOL.SetMode
 ***************************************/
EFI_STATUS
EFIAPI
LcdGraphicsSetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL   *This,
  IN UINT32                         ModeNumber
  )
{
  EFI_STATUS                      Status = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   FillColour;
  LCD_INSTANCE*                   Instance;

  Instance = LCD_INSTANCE_FROM_GOP_THIS (This);

  // Setup the hardware if not already done
  if(!mDisplayInitialized) {
    Status = InitializeDisplay (Instance);
    if (EFI_ERROR(Status)) {
      goto EXIT;
    }
  }

  // Check if this mode is supported
  if( ModeNumber >= This->Mode->MaxMode ) {
    DEBUG((DEBUG_ERROR, "LcdGraphicsSetMode: ERROR - Unsupported mode number %d .\n", ModeNumber ));
    Status = EFI_UNSUPPORTED;
    goto EXIT;
  }

  // Set the oscillator frequency to support the new mode
  Status = LcdPlatformSetMode (ModeNumber);
  if (EFI_ERROR(Status)) {
    Status = EFI_DEVICE_ERROR;
    goto EXIT;
  }

  // Update the UEFI mode information
  This->Mode->Mode = ModeNumber;
  LcdPlatformQueryMode (ModeNumber,&Instance->ModeInfo);
  This->Mode->FrameBufferSize =  Instance->ModeInfo.VerticalResolution
                               * Instance->ModeInfo.PixelsPerScanLine
                               * 4;

  // Set the hardware to the new mode
  Status = LcdSetMode (ModeNumber);
  if (EFI_ERROR(Status)) {
    Status = EFI_DEVICE_ERROR;
    goto EXIT;
  }

  BltLibConfigure (
    (VOID*)(UINTN) This->Mode->FrameBufferBase,
    This->Mode->Info
    );

  // The UEFI spec requires that we now clear the visible portions of the output display to black.

  // Set the fill colour to black
  SetMem (&FillColour, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL), 0x0);

  // Fill the entire visible area with the same colour.
  Status = This->Blt (
      This,
      &FillColour,
      EfiBltVideoFill,
      0,
      0,
      0,
      0,
      This->Mode->Info->HorizontalResolution,
      This->Mode->Info->VerticalResolution,
      0);

EXIT:
  return Status;
}

UINTN
GetBytesPerPixel (
  VOID
  )
{
  switch(LcdGetPixelFormat()) {
    case LKAPI_LCD_PIXELFORMAT_RGB888:
      return 3;
    case LKAPI_LCD_PIXELFORMAT_RGB565:
      return 2;
    default:
      return 0;
  }
}

UINTN
GetPixelMask (
  VOID
  )
{
  return 0;
}

UINT32
LKDisplayGetDensity (
  IN EFI_LK_DISPLAY_PROTOCOL* This
)
{
  return LKApi->lcd_get_density();
}

VOID
LKDisplaySetFlushMode (
  IN EFI_LK_DISPLAY_PROTOCOL* This,
  IN LK_DISPLAY_FLUSH_MODE Mode
)
{
  gLCDFlushMode = Mode;
}

LK_DISPLAY_FLUSH_MODE
LKDisplayGetFlushMode (
  IN EFI_LK_DISPLAY_PROTOCOL* This
)
{
  return gLCDFlushMode;
}

VOID
LcdCopy (
  IN LCD_INSTANCE *Instance
)
{
  UINT32          SourcePixelX;
  UINT32          DestinationPixelX;
  UINT32          SourceLine;
  UINT32          DestinationLine;
  UINT8           *SourcePixel;
  UINT8           *DestinationPixel;
  UINTN           BytesPerPixel;
  UINT32          HorizontalResolution;
  UINT32          VerticalResolution;
  UINT32          DestinationStride;
  UINT32          Index;

  UINT8* HWBuffer = (VOID*)(UINT32)Instance->FrameBufferBase;
  UINT8* SWBuffer = (VOID*)(UINT32)Instance->Gop.Mode->FrameBufferBase;

  BytesPerPixel = GetBytesPerPixel();
  HorizontalResolution = Instance->ModeInfo.HorizontalResolution;
  VerticalResolution = Instance->ModeInfo.VerticalResolution;

  if (Instance->Gop.Mode->Mode == 0)
    DestinationStride = HorizontalResolution;
  else
    DestinationStride = VerticalResolution;

  // Access each pixel inside the BltBuffer Memory
  for (SourceLine = 0; SourceLine < VerticalResolution; SourceLine++)
  {
    for (SourcePixelX = 0; SourcePixelX < HorizontalResolution; SourcePixelX++)
    {
      // RIGHT
      //DestinationLine = HorizontalResolution-SourcePixelX;
      //DestinationPixelX = SourceLine;

      if (Instance->Gop.Mode->Mode == 0) {
        DestinationLine = SourceLine;
        DestinationPixelX = SourcePixelX;
      }
      else {
        // LEFT
        DestinationLine = SourcePixelX;
        DestinationPixelX = VerticalResolution-SourceLine;
      }

      // Calculate the source and target addresses:
      SourcePixel      = SWBuffer + SourceLine      * HorizontalResolution * 4             + SourcePixelX      * 4;
      DestinationPixel = HWBuffer + DestinationLine * DestinationStride    * BytesPerPixel + DestinationPixelX * BytesPerPixel;

      // copy pixel
      // TODO: check pixel format
      for(Index = 0; Index<BytesPerPixel; Index++)
        DestinationPixel[Index] = SourcePixel[Index];
    }
  }
}

VOID
LKDisplayFlushScreen (
  IN EFI_LK_DISPLAY_PROTOCOL* This
)
{
  LCD_INSTANCE *Instance;
  EFI_TPL      OldTpl;
  UINT64       Now = 0;
  STATIC UINT64 RenderTime = 0;

  Instance = LCD_INSTANCE_FROM_LKDISPLAY_THIS(This);

  // limit flushes per second
  if(gLCDFlushMode==LK_DISPLAY_FLUSH_MODE_AUTO) {
    Now = GetTimeMs();
    if (Now-mLastFlush<RenderTime) {
      gLcdNeedsSync = TRUE;
      return;
    }
  }

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  // copy temporary to real framebuffer
  LcdCopy(Instance);

  // trigger hw flush
  LKApi->lcd_flush();

  if(gLCDFlushMode==LK_DISPLAY_FLUSH_MODE_AUTO) {
    mLastFlush = GetTimeMs();
    RenderTime = mLastFlush - Now;
    gLcdNeedsSync = FALSE;
  }

  gBS->RestoreTPL (OldTpl);
}

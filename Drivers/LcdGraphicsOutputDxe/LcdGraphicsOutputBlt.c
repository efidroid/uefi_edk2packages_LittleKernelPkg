/** @file

 Copyright (c) 2011-2013, ARM Ltd. All rights reserved.<BR>
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

extern BOOLEAN mDisplayInitialized;

EFI_STATUS
EFIAPI
LcdGraphicsBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL        *This,
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *BltBuffer,     OPTIONAL
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION   BltOperation,
  IN UINTN                               SourceX,
  IN UINTN                               SourceY,
  IN UINTN                               DestinationX,
  IN UINTN                               DestinationY,
  IN UINTN                               Width,
  IN UINTN                               Height,
  IN UINTN                               Delta           OPTIONAL   // Number of BYTES in a row of the BltBuffer
  )
{
  EFI_STATUS                      Status;
  EFI_TPL                         OriginalTPL;
  LCD_INSTANCE                    *Instance;

  Instance = LCD_INSTANCE_FROM_GOP_THIS(This);

  // Setup the hardware if not already done
  if (!mDisplayInitialized) {
    Status = InitializeDisplay (Instance);
    if (EFI_ERROR(Status)) {
      goto EXIT;
    }
  }

  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  switch (BltOperation) {
  case EfiBltVideoToBltBuffer:
  case EfiBltBufferToVideo:
  case EfiBltVideoFill:
  case EfiBltVideoToVideo:
    Status = BltLibGopBlt (
      BltBuffer,
      BltOperation,
      SourceX,
      SourceY,
      DestinationX,
      DestinationY,
      Width,
      Height,
      Delta
      );
    break;

  default:
    Status = EFI_INVALID_PARAMETER;
    ASSERT (FALSE);
  }

  if (!EFI_ERROR(Status) && BltOperation!=EfiBltVideoToBltBuffer) {
    if (gLCDFlushMode==LK_DISPLAY_FLUSH_MODE_AUTO) {
      Instance->LKDisplay.FlushScreen(&Instance->LKDisplay);
    }
  }

  gBS->RestoreTPL (OriginalTPL);

EXIT:
  return Status;
}

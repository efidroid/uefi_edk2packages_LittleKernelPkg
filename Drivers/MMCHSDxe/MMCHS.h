/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _MMCHS_H_
#define _MMCHS_H_

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/EmbeddedExternalDevice.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#include <LittleKernel.h>

//
// Device structures
//
typedef struct {
  VENDOR_DEVICE_PATH  Mmc;
  EFI_DEVICE_PATH     End;
} MMCHS_DEVICE_PATH;

typedef struct {
  UINT32                                Signature;
  EFI_HANDLE                            Handle;
  EFI_BLOCK_IO_PROTOCOL                 BlockIo;
  EFI_BLOCK_IO_MEDIA                    BlockMedia;
  MMCHS_DEVICE_PATH                     DevicePath;
  lkapi_biodev_t                        LKDev;
} BIO_INSTANCE;

#define BIO_INSTANCE_SIGNATURE  SIGNATURE_32('e', 'm', 'm', 'c')

#define BIO_INSTANCE_FROM_GOP_THIS(a)     CR (a, BIO_INSTANCE, BlockIo, BIO_INSTANCE_SIGNATURE)

//
// Function Prototypes
//

EFI_STATUS
EFIAPI
MMCHSReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  );

EFI_STATUS
EFIAPI
MMCHSReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  );

EFI_STATUS
EFIAPI
MMCHSWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  );

EFI_STATUS
EFIAPI
MMCHSFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  );

#endif

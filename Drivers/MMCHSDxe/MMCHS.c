/** @file
  MMC/SD Card driver for LittleKernel

  This driver always produces a BlockIo protocol but it starts off with no Media
  present. A TimerCallBack detects when media is inserted or removed and after
  a media change event a call to BlockIo ReadBlocks/WriteBlocks will cause the
  media to be detected (or removed) and the BlockIo Media structure will get
  updated. No MMC/SD Card harward registers are updated until the first BlockIo
  ReadBlocks/WriteBlocks after media has been insterted (booting with a card
  plugged in counts as an insertion event).

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "MMCHS.h"

lkapi_t* LKApi = NULL;

BIO_INSTANCE mBioTemplate = {
  BIO_INSTANCE_SIGNATURE,
  NULL, // Handle
  { // BlockIo
    EFI_BLOCK_IO_INTERFACE_REVISION,   // Revision
    NULL,                              // *Media
    MMCHSReset,                        // Reset
    MMCHSReadBlocks,                   // ReadBlocks
    MMCHSWriteBlocks,                  // WriteBlocks
    MMCHSFlushBlocks                   // FlushBlocks
  },
  { // BlockMedia
    BIO_INSTANCE_SIGNATURE,                   // MediaId
    FALSE,                                    // RemovableMedia
    TRUE,                                     // MediaPresent
    FALSE,                                    // LogicalPartition
    FALSE,                                    // ReadOnly
    FALSE,                                    // WriteCaching
    0,                                        // BlockSize
    4,                                        // IoAlign
    0,                                        // Pad
    0                                         // LastBlock
  },
  { // DevicePath
   {
      {
        HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
        { (UINT8) (sizeof(VENDOR_DEVICE_PATH)), (UINT8) ((sizeof(VENDOR_DEVICE_PATH)) >> 8) },
      },
      // Hardware Device Path for Bio
      EFI_CALLER_ID_GUID // Use the driver's GUID
    },

    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 }
    }
  },
  { // LKDev
    0,                           // type
    0,                           // block_size
    0,                           // num_blocks
    NULL,                        // init
    NULL,                        // read
    NULL,                        // write
  }
};

/**

  Reset the Block Device.



  @param  This                 Indicates a pointer to the calling context.

  @param  ExtendedVerification Driver may perform diagnostics on reset.



  @retval EFI_SUCCESS          The device was reset.

  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could

                               not be reset.



**/
EFI_STATUS
EFIAPI
MMCHSReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  )
{
  return EFI_SUCCESS;
}


/**

  Read BufferSize bytes from Lba into Buffer.



  @param  This       Indicates a pointer to the calling context.

  @param  MediaId    Id of the media, changes every time the media is replaced.

  @param  Lba        The starting Logical Block Address to read from

  @param  BufferSize Size of Buffer, must be a multiple of device block size.

  @param  Buffer     A pointer to the destination buffer for the data. The caller is

                     responsible for either having implicit or explicit ownership of the buffer.



  @retval EFI_SUCCESS           The data was read correctly from the device.

  @retval EFI_DEVICE_ERROR      The device reported an error while performing the read.

  @retval EFI_NO_MEDIA          There is no media in the device.

  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.

  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.

  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not valid,

                                or the buffer is not on proper alignment.

EFI_STATUS

**/
EFI_STATUS
EFIAPI
MMCHSReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  BIO_INSTANCE              *Instance;
  EFI_BLOCK_IO_MEDIA        *Media;
  UINTN                      BlockSize;

  Instance = BIO_INSTANCE_FROM_GOP_THIS(This);
  Media     = &Instance->BlockMedia;
  BlockSize = Media->BlockSize;

  if (MediaId != Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Lba > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Lba + (BufferSize / BlockSize) - 1) > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize % BlockSize != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  return Instance->LKDev.read(&Instance->LKDev, Lba, BufferSize, Buffer)==0?EFI_SUCCESS:EFI_DEVICE_ERROR;
}


/**

  Write BufferSize bytes from Lba into Buffer.



  @param  This       Indicates a pointer to the calling context.

  @param  MediaId    The media ID that the write request is for.

  @param  Lba        The starting logical block address to be written. The caller is

                     responsible for writing to only legitimate locations.

  @param  BufferSize Size of Buffer, must be a multiple of device block size.

  @param  Buffer     A pointer to the source buffer for the data.



  @retval EFI_SUCCESS           The data was written correctly to the device.

  @retval EFI_WRITE_PROTECTED   The device can not be written to.

  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.

  @retval EFI_NO_MEDIA          There is no media in the device.

  @retval EFI_MEDIA_CHNAGED     The MediaId does not matched the current device.

  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.

  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not valid,

                                or the buffer is not on proper alignment.



**/
EFI_STATUS
EFIAPI
MMCHSWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  )
{
  BIO_INSTANCE              *Instance;
  EFI_BLOCK_IO_MEDIA        *Media;
  UINTN                      BlockSize;

  Instance = BIO_INSTANCE_FROM_GOP_THIS(This);
  Media     = &Instance->BlockMedia;
  BlockSize = Media->BlockSize;

  if (MediaId != Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Lba > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Lba + (BufferSize / BlockSize) - 1) > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize % BlockSize != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  return Instance->LKDev.write(&Instance->LKDev, Lba, BufferSize, Buffer)==0?EFI_SUCCESS:EFI_DEVICE_ERROR;
}


/**

  Flush the Block Device.



  @param  This              Indicates a pointer to the calling context.



  @retval EFI_SUCCESS       All outstanding data was written to the device

  @retval EFI_DEVICE_ERROR  The device reported an error while writting back the data

  @retval EFI_NO_MEDIA      There is no media in the device.



**/
EFI_STATUS
EFIAPI
MMCHSFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}


EFI_BLOCK_IO_PROTOCOL gBlockIoTemplate = {
  EFI_BLOCK_IO_INTERFACE_REVISION,   // Revision
  NULL,                              // *Media
  MMCHSReset,                        // Reset
  MMCHSReadBlocks,                   // ReadBlocks
  MMCHSWriteBlocks,                  // WriteBlocks
  MMCHSFlushBlocks                   // FlushBlocks
};

EFI_STATUS
BioInstanceContructor (
  OUT BIO_INSTANCE** NewInstance
  )
{
  BIO_INSTANCE* Instance;

  Instance = AllocateCopyPool (sizeof(BIO_INSTANCE), &mBioTemplate);
  if (Instance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Instance->BlockIo.Media     = &Instance->BlockMedia;

  *NewInstance = Instance;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MMCHSInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  INT32       Count;
  UINTN       Index;
  BIO_INSTANCE    *Instance;
  lkapi_biodev_t  *Devices = NULL;
  EFI_GUID        VNOR_GUID = gLKVNORGuid;

  LKApi = GetLKApi();

  Count = LKApi->bio_list(NULL);
  Devices = (lkapi_biodev_t*)AllocateZeroPool (sizeof(lkapi_biodev_t) * Count);
  LKApi->bio_list(Devices);

  for (Index = 0 ; Index < Count ; Index++) {
    // Initialize device
    if (Devices[Index].init(&Devices[Index])) {
     Status = EFI_DEVICE_ERROR;
     goto EXIT;
    }

    // allocate instance
    Status = BioInstanceContructor (&Instance);
    if (EFI_ERROR(Status)) {
      goto EXIT;
    }

    // set data
    Instance->BlockMedia.BlockSize = Devices[Index].block_size;
    Instance->BlockMedia.LastBlock = Devices[Index].num_blocks - 1;
    Instance->LKDev = Devices[Index];
    // give every device a slighty different GUID, this limits us to 256 devices
    Instance->DevicePath.Mmc.Guid.Data4[7] = Index;
    if (Devices[Index].type == LKAPI_BIODEV_TYPE_VNOR)
      Instance->DevicePath.Mmc.Guid = VNOR_GUID;

    // Publish BlockIO
    Status = gBS->InstallMultipleProtocolInterfaces (
                &Instance->Handle,
                &gEfiBlockIoProtocolGuid,    &Instance->BlockIo,
                &gEfiDevicePathProtocolGuid, &Instance->DevicePath,
                NULL
                );
  }

  EXIT:
  if (Devices)
    FreePool(Devices);

  return Status;
}

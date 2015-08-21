/** @file
  Save Non-Volatile Variables to a file system.

  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "NvVarsBlockIoLib.h"
#include <Library/DebugLib.h>
#include <Library/NvVarsBlockIoLib.h>

EFI_HANDLE            mNvVarsLibBlockIoHandle = NULL;
EFI_BLOCK_IO_PROTOCOL *mBlockIo = NULL;


/**
  Attempts to connect the NvVarsFileLib to the specified BlockIo protocol.

  @param[in]  FsHandle - Handle for a gEfiBlockIoProtocolGuid instance

  @return     The EFI_STATUS while attempting to connect the NvVarsLib
              to the protocol instance.
  @retval     EFI_SUCCESS - The given interface was connected successfully

**/
EFI_STATUS
EFIAPI
ConnectNvVarsToBlockIo (
  IN EFI_HANDLE BlockIoHandle
  )
{
  EFI_STATUS Status;

  //
  // Get the BlockIO protocol on that handle
  //
  Status = gBS->HandleProtocol (
                  BlockIoHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&mBlockIo
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // We might fail to load the variable, since the BlockIo initially
  // will contain valid NvVars data.
  //
  LoadNvVarsFromBlockIo (mBlockIo);

  //
  // We must be able to save the variables successfully to the BlockIo
  // to have connected successfully.
  //
  Status = SaveNvVarsToBlockIo (mBlockIo);
  if (!EFI_ERROR (Status)) {
    mNvVarsLibBlockIoHandle = BlockIoHandle;
  }

  return Status;
}


/**
  write non-volatile variables to the device.

  @return     The EFI_STATUS while attempting to write the variable to
              the connected BlockIo.
  @retval     EFI_SUCCESS - The non-volatile variables were saved to the device
  @retval     EFI_NOT_STARTED - A interface has not been connected

**/
EFI_STATUS
EFIAPI
WriteNvVarsToBlockIo (
  )
{
  if (mNvVarsLibBlockIoHandle == NULL) {
    //
    // A file system had not been connected to the library.
    //
    return EFI_NOT_STARTED;
  } else {
    return SaveNvVarsToBlockIo (mBlockIo);
  }
}



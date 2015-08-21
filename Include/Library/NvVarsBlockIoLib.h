/** @file
  Provides functions to save and restore NV variables in a file.

  Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __NV_VARS_BLOCK_IO_LIB__
#define __NV_VARS_BLOCK_IO_LIB__

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
  IN EFI_HANDLE    BlockIoHandle
  );


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
  );


#endif


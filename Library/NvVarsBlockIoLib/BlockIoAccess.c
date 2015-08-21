/** @file
  File System Access for NvVarsFileLib

  Copyright (c) 2004 - 2014, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "NvVarsBlockIoLib.h"

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>

#define NVVARS_SIGNATURE SIGNATURE_32('n', 'v', 'i', 'o')


/**
  Reads the contents of the NvVars data from BlockIo

  @param[in]  BlockIo - The BlockIO to read from

  @return     EFI_STATUS based on the success or failure of the data read

**/
EFI_STATUS
ReadNvVars (
  IN  EFI_BLOCK_IO_PROTOCOL            *BlockIo
  )
{
  EFI_STATUS                  Status;
  UINTN                       DataSize;
  UINTN                       ReadSize;
  VOID                        *FileContents;
  EFI_HANDLE                  SerializedVariables;
  UINT32                      *BufPtr32;

  // allocate buffer for first block
  FileContents = AllocatePool (BlockIo->Media->BlockSize);
  if (FileContents == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // read first block
  Status = BlockIo->ReadBlocks(
               BlockIo,
               BlockIo->Media->MediaId,
               0,
               BlockIo->Media->BlockSize,
               FileContents
               );
  if (EFI_ERROR (Status)) {
    FreePool (FileContents);
    return Status;
  }
  BufPtr32 = (UINT32*)FileContents;

  // check signature
  if (BufPtr32[0] != NVVARS_SIGNATURE) {
    FreePool (FileContents);
    return EFI_UNSUPPORTED;
  }

  // get data size
  DataSize = BufPtr32[1];
  ReadSize = ALIGN_VALUE(sizeof(UINT32)*2 + DataSize, BlockIo->Media->BlockSize);

  // increase buffer size to fit all data
  FreePool (FileContents);
  FileContents = AllocatePool (ReadSize);
  if (FileContents == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // read all data
  Status = BlockIo->ReadBlocks(
               BlockIo,
               BlockIo->Media->MediaId,
               0,
               ReadSize,
               FileContents
               );
  if (EFI_ERROR (Status)) {
    FreePool (FileContents);
    return Status;
  }

  DEBUG ((
    EFI_D_INFO,
    "FsAccess.c: Read %Lu bytes from NV Variables interface\n",
    (UINT64)DataSize
    ));

  Status = SerializeVariablesNewInstanceFromBuffer (
             &SerializedVariables,
             FileContents + sizeof(UINT32)*2,
             DataSize
             );
  if (!RETURN_ERROR (Status)) {
    Status = SerializeVariablesSetSerializedVariables (SerializedVariables);
  }

  FreePool (FileContents);

  return Status;
}


/**
  Writes a variable to indicate that the NV variables
  have been loaded from the file system.

**/
STATIC
VOID
SetNvVarsVariable (
  VOID
  )
{
  BOOLEAN                        VarData;
  UINTN                          Size;

  //
  // Write a variable to indicate we've already loaded the
  // variable data.  If it is found, we skip the loading on
  // subsequent attempts.
  //
  Size = sizeof (VarData);
  VarData = TRUE;
  gRT->SetVariable (
         L"NvVars",
         &gEfiBlockIoProtocolGuid,
         EFI_VARIABLE_NON_VOLATILE |
           EFI_VARIABLE_BOOTSERVICE_ACCESS |
           EFI_VARIABLE_RUNTIME_ACCESS,
         Size,
         (VOID*) &VarData
         );
}


/**
  Loads the non-volatile variables from the BlockIo interface.

  @param[in]  BlockIo - The BlockIo to read from

  @return     EFI_STATUS based on the success or failure of load operation

**/
EFI_STATUS
LoadNvVarsFromBlockIo (
  EFI_BLOCK_IO_PROTOCOL                     *BlockIo
  )
{
  EFI_STATUS                     Status;
  BOOLEAN                        VarData;
  UINTN                          Size;

  DEBUG ((EFI_D_INFO, "FsAccess.c: LoadNvVarsFromFs\n"));

  //
  // We write a variable to indicate we've already loaded the
  // variable data.  If it is found, we skip the loading.
  //
  // This is relevent if the non-volatile variable have been
  // able to survive a reboot operation.  In that case, we don't
  // want to re-load the file as it would overwrite newer changes
  // made to the variables.
  //
  Size = sizeof (VarData);
  VarData = TRUE;
  Status = gRT->GetVariable (
                  L"NvVars",
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &Size,
                  (VOID*) &VarData
                  );
  if (Status == EFI_SUCCESS) {
    DEBUG ((EFI_D_INFO, "NV Variables were already loaded\n"));
    return EFI_ALREADY_STARTED;
  }

  //
  // Attempt to restore the variables from the BlockIo interface.
  //
  Status = ReadNvVars (BlockIo);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "Error while restoring NV variable data\n"));
    return Status;
  }

  //
  // Write a variable to indicate we've already loaded the
  // variable data.  If it is found, we skip the loading on
  // subsequent attempts.
  //
  SetNvVarsVariable();

  DEBUG ((
    EFI_D_INFO,
    "FsAccess.c: Read NV Variables file (size=%Lu)\n",
    (UINT64)Size
    ));

  return Status;
}


STATIC
RETURN_STATUS
EFIAPI
IterateVariablesCallbackAddAllNvVariables (
  IN  VOID                         *Context,
  IN  CHAR16                       *VariableName,
  IN  EFI_GUID                     *VendorGuid,
  IN  UINT32                       Attributes,
  IN  UINTN                        DataSize,
  IN  VOID                         *Data
  )
{
  EFI_HANDLE  Instance;

  Instance = (EFI_HANDLE) Context;

  //
  // Only save non-volatile variables
  //
  if ((Attributes & EFI_VARIABLE_NON_VOLATILE) == 0) {
    return RETURN_SUCCESS;
  }

  return SerializeVariablesAddVariable (
           Instance,
           VariableName,
           VendorGuid,
           Attributes,
           DataSize,
           Data
           );
}


/**
  Saves the non-volatile variables to the BlockIo interface.

  @param[in]  BlockIoHandle - Handle for a gEfiBlockIoProtocolGuid instance

  @return     EFI_STATUS based on the success or failure of load operation

**/
EFI_STATUS
SaveNvVarsToBlockIo (
  EFI_BLOCK_IO_PROTOCOL                            *BlockIo
  )
{
  EFI_STATUS                  Status;
  UINTN                       WriteSize;
  UINTN                       VariableDataSize;
  UINTN                       BufferSize;
  VOID                        *VariableData;
  EFI_HANDLE                  SerializedVariables;

  SerializedVariables = NULL;

  Status = SerializeVariablesNewInstance (&SerializedVariables);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SerializeVariablesIterateSystemVariables (
             IterateVariablesCallbackAddAllNvVariables,
             (VOID*) SerializedVariables
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  VariableData = NULL;
  VariableDataSize = 0;
  BufferSize = 0;
  Status = SerializeVariablesToBuffer (
             SerializedVariables,
             NULL,
             &VariableDataSize
             );
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    BufferSize = ALIGN_VALUE(sizeof(UINT32)*2 + VariableDataSize, BlockIo->Media->BlockSize);
    VariableData = AllocatePool (BufferSize);
    if (VariableData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
    } else {
      UINT32* BufPtr = (UINT32*)VariableData;
      BufPtr[0] = NVVARS_SIGNATURE;
      BufPtr[1] = VariableDataSize;

      Status = SerializeVariablesToBuffer (
                 SerializedVariables,
                 VariableData + sizeof(UINT32)*2,
                 &VariableDataSize
                 );
    }
  }

  SerializeVariablesFreeInstance (SerializedVariables);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  WriteSize = BufferSize;
  Status = BlockIo->WriteBlocks(
               BlockIo,
               BlockIo->Media->MediaId,
               0,
               WriteSize,
               VariableData
               );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (!EFI_ERROR (Status)) {
    //
    // Write a variable to indicate we've already loaded the
    // variable data.  If it is found, we skip the loading on
    // subsequent attempts.
    //
    SetNvVarsVariable();

    DEBUG ((EFI_D_INFO, "Saved NV Variables to NvVars BlockIo\n"));
  }

  return Status;

}



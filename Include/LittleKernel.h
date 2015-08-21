#ifndef __LITTLEKERNEL_PLATFORM_H__
#define __LITTLEKERNEL_PLATFORM_H__

#include <Library/PcdLib.h>
#include <Uefi/UefiBaseType.h>
#include <LittleKernelApi.h>

extern EFI_GUID gLKApiAddrGuid;
extern EFI_GUID gLKVNORGuid;

/**
  Returns the pointer to the LK API.

  @return The pointer to the LK API.

**/

lkapi_t *
EFIAPI
GetLKApi (
  VOID
  );



/**
  Creates the Hob for the LK API.

  @param  HobList       Hob list pointer to store

**/
EFI_STATUS
EFIAPI
BuildLKApiHob (
  VOID
  );

#endif

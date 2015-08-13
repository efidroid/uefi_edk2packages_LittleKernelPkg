#ifndef __LITTLEKERNEL_PLATFORM_H__
#define __LITTLEKERNEL_PLATFORM_H__

#include <Library/PcdLib.h>
#include <Uefi/UefiBaseType.h>
#include <LittleKernelApi.h>

/**
  Returns the pointer to the LK API.

  @return The pointer to the LK APO.

**/

VOID *
EFIAPI
GetLKApi (
  VOID
  );



/**
  Updates the pointer to the LK API.

  @param  HobList       Hob list pointer to store

**/
EFI_STATUS
EFIAPI
SetLKApi (
  IN  VOID      *LKApi
  );

#endif

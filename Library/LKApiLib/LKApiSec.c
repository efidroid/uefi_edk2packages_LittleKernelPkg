#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <LittleKernel.h>

// LIBLK functions
extern lkapi_t lk_uefiapi;

/**
  Returns the pointer to the LK API

  @return pointer to the LK API

**/
lkapi_t *
EFIAPI
GetLKApi (
  VOID
  )
{
  return &lk_uefiapi;
}

/**
  Updates the pointer to the LK API.

  @param  LKApi       pointer to the LK API

**/
EFI_STATUS
EFIAPI
BuildLKApiHob (
  VOID
  )
{
  UINT64 *LKApiHobData;

  LKApiHobData = BuildGuidHob (&gLKApiAddrGuid, sizeof *LKApiHobData);
  ASSERT (LKApiHobData != NULL);
  *LKApiHobData = (UINTN)&lk_uefiapi;

  return EFI_SUCCESS;
}


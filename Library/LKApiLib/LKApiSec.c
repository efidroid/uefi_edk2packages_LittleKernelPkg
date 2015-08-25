#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <LittleKernel.h>

// LIBLK functions
extern lkapi_t* LKApiAddr;

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
  return LKApiAddr;
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
  *LKApiHobData = (UINTN)LKApiAddr;

  return EFI_SUCCESS;
}


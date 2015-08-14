#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <LittleKernel.h>

STATIC lkapi_t* mLKApi = NULL;

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
  VOID                *Hob;
  CONST UINT64        *LKApiHobData;

  if (mLKApi != NULL)
    return mLKApi;

  Hob = GetFirstGuidHob (&gLKApiAddrGuid);
  if (Hob == NULL || GET_GUID_HOB_DATA_SIZE (Hob) != sizeof *LKApiHobData) {
    return NULL;
  }
  LKApiHobData = GET_GUID_HOB_DATA (Hob);

  mLKApi = (lkapi_t*)(UINTN)*LKApiHobData;
  if (mLKApi == NULL) {
    return NULL;
  }

  return mLKApi;
}

/**
  Updates the pointer to the LK API.

  @param  LKApi       pointer to the LK API

**/
EFI_STATUS
EFIAPI
SetLKApi (
  IN  lkapi_t      *LKApi
  )
{
  UINT64 *LKApiHobData;
  mLKApi = LKApi;

  LKApiHobData = BuildGuidHob (&gLKApiAddrGuid, sizeof *LKApiHobData);
  ASSERT (LKApiHobData != NULL);
  *LKApiHobData = (UINTN)mLKApi;

  return EFI_SUCCESS;
}


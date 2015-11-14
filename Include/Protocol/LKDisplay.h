#ifndef __LK_DISPLAY_H__
#define __LK_DISPLAY_H__

#include <Uefi/UefiSpec.h>

#define EFI_LK_DISPLAY_PROTOCOL_GUID \
  { \
    0xc2217e7d, 0x6853, 0x4c3f, {0xae, 0x97, 0xaf, 0xbc, 0x85, 0xc9, 0xa1, 0xe0 } \
  }

typedef struct _EFI_LK_DISPLAY_PROTOCOL  EFI_LK_DISPLAY_PROTOCOL;

typedef enum {
  LK_DISPLAY_FLUSH_MODE_AUTO = 0,
  LK_DISPLAY_FLUSH_MODE_MANUAL,
} LK_DISPLAY_FLUSH_MODE;

struct _EFI_LK_DISPLAY_PROTOCOL {
  UINT32 (*GetDensity)(EFI_LK_DISPLAY_PROTOCOL*);
  VOID   (*SetFlushMode)(EFI_LK_DISPLAY_PROTOCOL*, LK_DISPLAY_FLUSH_MODE);
  LK_DISPLAY_FLUSH_MODE (*GetFlushMode)(EFI_LK_DISPLAY_PROTOCOL*);
  VOID   (*FlushScreen)(EFI_LK_DISPLAY_PROTOCOL*);
  UINT32 (*GetPortraitMode)(VOID);
  UINT32 (*GetLandscapeMode)(VOID);
};

extern EFI_GUID gEfiLKDisplayProtocolGuid;

#endif


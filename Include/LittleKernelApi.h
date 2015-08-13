#ifndef __LITTLEKERNELAPI_PLATFORM_H__
#define __LITTLEKERNELAPI_PLATFORM_H__

typedef struct {
	int (*serial_write_char)(char c);
} lkapi_t;

#endif

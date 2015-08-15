#ifndef __LITTLEKERNELAPI_PLATFORM_H__
#define __LITTLEKERNELAPI_PLATFORM_H__

typedef struct {
	void (*platform_early_init)(void);
	void (*serial_write_char)(char c);
} lkapi_t;

#endif

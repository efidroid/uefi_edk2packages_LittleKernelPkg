#ifndef __LITTLEKERNELAPI_PLATFORM_H__
#define __LITTLEKERNELAPI_PLATFORM_H__

typedef struct {
	void (*platform_early_init)(void);
	int (*serial_poll_char)(void);
	void (*serial_write_char)(char c);
	char (*serial_read_char)(void);
} lkapi_t;

#endif

#ifndef __LITTLEKERNELAPI_PLATFORM_H__
#define __LITTLEKERNELAPI_PLATFORM_H__

enum lkapi_handler_return {
	LKAPI_INT_NO_RESCHEDULE = 0,
	LKAPI_INT_RESCHEDULE,
};
typedef enum lkapi_handler_return (*lkapi_int_handler)(void *arg);

typedef void (*lkapi_timer_callback_t)(void);

typedef struct {
	unsigned int block_size;
	unsigned long long num_blocks;

	int (*init)(void);
	int (*read)(unsigned long long lba, unsigned long buffersize, void* buffer);
	int (*write)(unsigned long long lba, unsigned long buffersize, void* buffer);
} lkapi_biodev_t;

typedef struct {
	void (*platform_early_init)(void);

	int (*serial_poll_char)(void);
	void (*serial_write_char)(char c);
	char (*serial_read_char)(void);

	int (*timer_register_handler)(lkapi_timer_callback_t handler);
	void (*timer_set_period)(unsigned long long period);
	void (*timer_delay_microseconds)(unsigned int microseconds);
	void (*timer_delay_nanoseconds)(unsigned int nanoseconds);

	int (*int_mask)(unsigned int vector);
	int (*int_unmask)(unsigned int vector);
	void (*int_register_handler)(unsigned int vector, lkapi_int_handler func, void *arg);


	int (*bio_list)(lkapi_biodev_t* list);

	unsigned long long (*lcd_get_vram_address)(void);
	int (*lcd_init)(unsigned long long vramaddr);
	unsigned int (*lcd_get_width)(void);
	unsigned int (*lcd_get_height)(void);
	void (*lcd_flush)(void);
	void (*lcd_shutdown)(void);

	void (*reset_cold)(void);
	void (*reset_warm)(void);
	void (*reset_shutdown)(void);

	int (*rtc_init)(void);
	int (*rtc_gettime)(unsigned int* time);
	int (*rtc_settime)(unsigned int time);
} lkapi_t;

#endif

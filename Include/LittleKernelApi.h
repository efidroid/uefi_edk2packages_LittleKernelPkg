#ifndef __LITTLEKERNELAPI_PLATFORM_H__
#define __LITTLEKERNELAPI_PLATFORM_H__

typedef enum {
	LKAPI_MEMORY_UNCACHED = 0,
	LKAPI_MEMORY_WRITE_BACK,
	LKAPI_MEMORY_WRITE_THROUGH,
	LKAPI_MEMORY_DEVICE,
} lkapi_memorytype_t;

enum lkapi_handler_return {
	LKAPI_INT_NO_RESCHEDULE = 0,
	LKAPI_INT_RESCHEDULE,
};
typedef enum lkapi_handler_return (*lkapi_int_handler)(void *arg);

typedef void (*lkapi_timer_callback_t)(void);

typedef enum {
	LKAPI_BIODEV_TYPE_MMC,
	LKAPI_BIODEV_TYPE_VNOR,
} lkapi_biodev_type_t;

typedef struct lkapi_biodev lkapi_biodev_t;
struct lkapi_biodev {
	lkapi_biodev_type_t type;
	unsigned int block_size;
	unsigned long long num_blocks;

	int (*init)(lkapi_biodev_t* dev);
	int (*read)(lkapi_biodev_t* dev, unsigned long long lba, unsigned long buffersize, void* buffer);
	int (*write)(lkapi_biodev_t* dev, unsigned long long lba, unsigned long buffersize, void* buffer);
};

typedef void* (*lkapi_mmap_cb_t)(void* pdata, unsigned long addr, unsigned long size, int reserved);
typedef void* (*lkapi_mmap_mappings_cb_t)(void* pdata, unsigned long vaddr, unsigned long paddr, unsigned long size, lkapi_memorytype_t type);

typedef enum {
	LKAPI_UEFI_BM_NORMAL = 0,
	LKAPI_UEFI_BM_RECOVERY,
} lkapi_uefi_bootmode;

typedef struct {
	void (*platform_early_init)(void);
	lkapi_uefi_bootmode (*platform_get_uefi_bootmode)(void);

	int (*serial_poll_char)(void);
	void (*serial_write_char)(char c);
	int (*serial_read_char)(char* c);

	int (*timer_register_handler)(lkapi_timer_callback_t handler);
	void (*timer_set_period)(unsigned long long period);
	void (*timer_delay_microseconds)(unsigned int microseconds);
	void (*timer_delay_nanoseconds)(unsigned int nanoseconds);

	int (*int_mask)(unsigned int vector);
	int (*int_unmask)(unsigned int vector);
	void (*int_register_handler)(unsigned int vector, lkapi_int_handler func, void *arg);
	unsigned int (*int_get_dist_base)(void);
	unsigned int (*int_get_redist_base)(void);
	unsigned int (*int_get_cpu_base)(void);

	int (*bio_list)(lkapi_biodev_t* list);

	unsigned long long (*lcd_get_vram_address)(void);
	int (*lcd_init)(void);
	unsigned int (*lcd_get_width)(void);
	unsigned int (*lcd_get_height)(void);
	unsigned int (*lcd_get_density)(void);
	void (*lcd_flush)(void);
	void (*lcd_shutdown)(void);

	void (*reset_cold)(void);
	void (*reset_warm)(void);
	void (*reset_shutdown)(void);

	int (*rtc_init)(void);
	int (*rtc_gettime)(unsigned int* time);
	int (*rtc_settime)(unsigned int time);

	void* (*mmap_get_dram)(void* pdata, lkapi_mmap_cb_t cb);
	void* (*mmap_get_mappings)(void* pdata, lkapi_mmap_mappings_cb_t cb);
	void (*mmap_get_lk_range)(unsigned long *addr, unsigned long *size);

	int (*boot_create_tags)(const char* cmdline, unsigned int ramdisk_addr, unsigned int ramdisk_size, unsigned int tags_addr, unsigned int tags_size);
	unsigned int (*boot_machine_type)(void);
	void (*boot_update_addrs)(unsigned int* kernel, unsigned int* ramdisk, unsigned int* tags);

	void (*event_init)(void** event);
	void (*event_destroy)(void* event);
	void (*event_wait)(void** event);
	void (*event_signal)(void* event);
} lkapi_t;

#endif

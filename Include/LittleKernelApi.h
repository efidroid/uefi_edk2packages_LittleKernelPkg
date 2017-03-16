#ifndef __LITTLEKERNELAPI_PLATFORM_H__
#define __LITTLEKERNELAPI_PLATFORM_H__

//
// mmap
//

#define LKAPI_MEMORYTYPE_RESERVED 0
#define LKAPI_MEMORYTYPE_LOADER_CODE 1
#define LKAPI_MEMORYTYPE_LOADER_DATA 2
#define LKAPI_MEMORYTYPE_BOOTSERVICES_CODE 3
#define LKAPI_MEMORYTYPE_BOOTSERVICES_DATA 4
#define LKAPI_MEMORYTYPE_RUNTIMESERVICES_CODE 5
#define LKAPI_MEMORYTYPE_RUNTIMESERVICES_DATA 6
#define LKAPI_MEMORYTYPE_CONVENTIONAL_MEMORY 7
#define LKAPI_MEMORYTYPE_UNUSABLE_MEMORY 8
#define LKAPI_MEMORYTYPE_MEMORY_MAPPED_IO 11
#define LKAPI_MEMORYTYPE_MEMORY_MAPPED_IO_PORTSPACE 12
#define LKAPI_MEMORYTYPE_PAL_CODE 13
#define LKAPI_MEMORYTYPE_PERSISTENT 14

#define LKAPI_MEMORYATTR_DONT_MAP -1
#define LKAPI_MEMORYATTR_UNCACHED 0
#define LKAPI_MEMORYATTR_WRITE_BACK 1
#define LKAPI_MEMORYATTR_WRITE_THROUGH 2
#define LKAPI_MEMORYATTR_DEVICE 3

#define LKAPI_MMAP_RANGEFLAG_UNUSED   1
#define LKAPI_MMAP_RANGEFLAG_DRAM     2
#define LKAPI_MMAP_RANGEFLAG_RESERVED 4
typedef unsigned int lkapi_mmap_rangeflags_t;

typedef void* (*lkapi_mmap_add_cb_t) (void *pdata, unsigned long long start, unsigned long long size, lkapi_mmap_rangeflags_t rangeflags,
                       long MemoryAttributes, long MemoryType, lkapi_mmap_rangeflags_t insert_in_rangeflags);


//
// boot
//

typedef void *(*lkapi_boot_mmap_cb_t)(void *pdata, unsigned long long addr, unsigned long long size);


//
// interrupts
//

#define LKAPI_INT_NO_RESCHEDULE 0
#define LKAPI_INT_RESCHEDULE 1


//
// BIO
//

#define LKAPI_BIODEV_TYPE_MMC 0
#define LKAPI_BIODEV_TYPE_VNOR 1

typedef unsigned int (*lkapi_int_handler)(void *arg);
typedef void (*lkapi_timer_callback_t)(void);

typedef struct lkapi_biodev lkapi_biodev_t;
struct lkapi_biodev {
    int id;
    unsigned int type;
    unsigned int block_size;
    unsigned long long num_blocks;
    void *api_pdata;

    int (*init)(lkapi_biodev_t *dev);
    int (*read)(lkapi_biodev_t *dev, unsigned long long lba, unsigned long buffersize, void *buffer);
    int (*write)(lkapi_biodev_t *dev, unsigned long long lba, unsigned long buffersize, void *buffer);
};


//
// USB
//

#define LKAPI_UEFI_BM_NORMAL 0
#define LKAPI_UEFI_BM_RECOVERY 1
#define LKAPI_UEFI_BM_CHARGER 2

#define LKAPI_LCD_PIXELFORMAT_INVALID -1
#define LKAPI_LCD_PIXELFORMAT_RGB888   0
#define LKAPI_LCD_PIXELFORMAT_RGB565   1

#define LKAPI_UDC_EVENT_ONLINE  1
#define LKAPI_UDC_EVENT_OFFLINE 2

typedef struct lkapi_udc_gadget lkapi_udc_gadget_t;
struct lkapi_udc_gadget {
    void (*notify)(lkapi_udc_gadget_t *gadget, unsigned event);

    unsigned char ifc_class;
    unsigned char ifc_subclass;
    unsigned char ifc_protocol;
    unsigned char ifc_endpoints;
    const char *ifc_string;
    unsigned flags;

    int (*usb_read)(lkapi_udc_gadget_t *gadget, void *buf, unsigned len);
    int (*usb_write)(lkapi_udc_gadget_t *gadget, void *buf, unsigned len);

    void *pdata;
};

typedef struct lkapi_udc_device lkapi_udc_device_t;
typedef struct lkapi_usbgadget_iface lkapi_usbgadget_iface_t;

struct lkapi_udc_device {
    unsigned short vendor_id;
    unsigned short product_id;
    unsigned short version_id;

    const char *manufacturer;
    const char *product;
    const char *serialno;
};

struct lkapi_usbgadget_iface {
    int (*udc_init)(lkapi_usbgadget_iface_t *dev, lkapi_udc_device_t *devinfo);
    int (*udc_register_gadget)(lkapi_usbgadget_iface_t *dev, lkapi_udc_gadget_t *gadget);
    int (*udc_start)(lkapi_usbgadget_iface_t *dev);
    int (*udc_stop)(lkapi_usbgadget_iface_t *dev);

    void *pdata;
};


//
// main callback structure
//
typedef struct {
    void (*platform_early_init)(void);
    void (*platform_init)(void);
    void (*platform_uninit)(void);
    unsigned int (*platform_get_uefi_bootmode)(void);
    const char* (*platform_get_uefi_bootpart)(void);

    int (*serial_poll_char)(void);
    void (*serial_write_char)(char c);
    int (*serial_read_char)(char *c);

    int (*timer_register_handler)(lkapi_timer_callback_t handler);
    void (*timer_set_period)(unsigned long long period);
    void (*timer_delay_microseconds)(unsigned int microseconds);
    void (*timer_delay_nanoseconds)(unsigned int nanoseconds);

    unsigned long long (*perf_ticks)(void);
    unsigned long long (*perf_props)(unsigned long long *startval, unsigned long long *endval);
    unsigned long long (*perf_ticks_to_ns)(unsigned long long ticks);

    int (*int_mask)(unsigned int vector);
    int (*int_unmask)(unsigned int vector);
    void (*int_register_handler)(unsigned int vector, lkapi_int_handler func, void *arg);
    unsigned int (*int_get_dist_base)(void);
    unsigned int (*int_get_redist_base)(void);
    unsigned int (*int_get_cpu_base)(void);

    int (*bio_list)(lkapi_biodev_t *list);

    unsigned long long (*lcd_get_vram_address)(void);
    unsigned long long (*lcd_get_vram_size)(void);
    int (*lcd_init)(void);
    unsigned int (*lcd_get_width)(void);
    unsigned int (*lcd_get_height)(void);
    unsigned int (*lcd_get_density)(void);
    unsigned int (*lcd_get_bpp)(void);
    int (*lcd_get_pixelformat)(void);
    int  (*lcd_needs_flush)(void);
    void (*lcd_flush)(void);
    void (*lcd_shutdown)(void);

    void (*reset_cold)(const char *reason);
    void (*reset_warm)(const char *reason);
    void (*reset_shutdown)(const char *reason);

    int (*rtc_init)(void);
    int (*rtc_gettime)(unsigned int *time);
    int (*rtc_settime)(unsigned int time);

    void *(*mmap_get_all)(void *pdata, lkapi_mmap_add_cb_t cb);
    int (*mmap_needs_identity_map)(void);

    void *(*boot_get_mmap)(void *pdata, lkapi_boot_mmap_cb_t cb);
    int (*boot_get_hwid)(const char* id, unsigned int* datap);
    const char* (*boot_get_default_fdt_parser)(void);
    const char* (*boot_get_cmdline_extension)(int is_recovery);
    void (*boot_update_addrs)(int is64, unsigned int *kernel, unsigned int *ramdisk, unsigned int *tags);
    void* (*boot_extend_atags)(void *atags);
    void (*boot_extend_fdt)(void *fdt);
    void (*boot_exec)(int is64, void *kernel, unsigned int zero, unsigned int arch, unsigned int tags);

    void (*event_init)(void **event);
    void (*event_destroy)(void *event);
    void (*event_wait)(void **event);
    void (*event_signal)(void *event);

    lkapi_usbgadget_iface_t *(*usbgadget_get_interface)(void);
} lkapi_t;

#endif

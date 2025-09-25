#include <linux/version.h>

/* Platform driver remove function signature changed in kernel 5.18+ 
 * However, Raspberry Pi kernels vary in their API adoption
 * Test for actual function signature rather than just version */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
    /* Very recent kernels (6.8+) generally use void */
    #define MATRIXIO_REMOVE_RETURN_TYPE void
    #define MATRIXIO_REMOVE_RETURN() return
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
    /* Kernel 5.18+ introduced void, but RPi kernels may vary */
    /* For safety, use int for RPi kernels until 6.8+ */
    #define MATRIXIO_REMOVE_RETURN_TYPE int  
    #define MATRIXIO_REMOVE_RETURN() return 0
#else
    /* Legacy kernels (5.10.x, etc.) use int return type */
    #define MATRIXIO_REMOVE_RETURN_TYPE int
    #define MATRIXIO_REMOVE_RETURN() return 0
#endif

/* Fix for GPIO function return types in kernel 6.12+ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    #define MATRIXIO_GPIO_RETURN_TYPE void
    #define MATRIXIO_GPIO_RETURN() return
#else  
    #define MATRIXIO_GPIO_RETURN_TYPE int
    #define MATRIXIO_GPIO_RETURN() return 0
#endif

/* UART xmit structure moved in kernel 6.1+ - access patterns changed */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include <linux/serial_core.h>
#endif

/* Class creation API changed in kernel 6.4+ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    #define MATRIXIO_CLASS_CREATE(name) class_create(name)
#else
    #define MATRIXIO_CLASS_CREATE(name) class_create(THIS_MODULE, name)
#endif

/* Device uevent function signature */
#define MATRIXIO_UEVENT_CAST(func) ((int (*)(const struct device *, struct kobj_uevent_env *))(func))

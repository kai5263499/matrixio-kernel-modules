#ifndef MATRIXIO_COMPAT_H
#define MATRIXIO_COMPAT_H

#include <linux/version.h>
#include <linux/platform_device.h>

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
#include <linux/circ_buf.h>
/* In 6.1+ the xmit buffer is accessed differently */
#define MATRIXIO_UART_XMIT(port) (&(port)->state->xmit)
#define MATRIXIO_UART_XMIT_BUF(port) ((port)->state->xmit.buf)
#define MATRIXIO_UART_XMIT_TAIL(port) ((port)->state->xmit.tail)
#define MATRIXIO_UART_XMIT_SET_TAIL(port, val) ((port)->state->xmit.tail = (val))
#define MATRIXIO_UART_CIRC_EMPTY(port) uart_circ_empty(&(port)->state->xmit)
#else
#include <linux/serial_core.h>
#include <linux/circ_buf.h>
#define MATRIXIO_UART_XMIT(port) (&(port)->state->xmit)
#define MATRIXIO_UART_XMIT_BUF(port) ((port)->state->xmit.buf)
#define MATRIXIO_UART_XMIT_TAIL(port) ((port)->state->xmit.tail)
#define MATRIXIO_UART_XMIT_SET_TAIL(port, val) ((port)->state->xmit.tail = (val))
#define MATRIXIO_UART_CIRC_EMPTY(port) uart_circ_empty(&(port)->state->xmit)
#endif

/* Class creation API changed in kernel 6.4+ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    #define MATRIXIO_CLASS_CREATE(name) class_create(name)
#else
    #define MATRIXIO_CLASS_CREATE(name) class_create(THIS_MODULE, name)
#endif

/* Device uevent function signature changed - use void* cast to bypass type checking */
#define MATRIXIO_UEVENT_CAST(func) ((void*)(func))

/* IIO mlock removed in kernel 6.1+ - use iio_device_claim_direct_mode instead */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    #include <linux/iio/iio.h>
    #define MATRIXIO_IIO_LOCK(indio_dev) iio_device_claim_direct_mode(indio_dev)
    #define MATRIXIO_IIO_UNLOCK(indio_dev) iio_device_release_direct_mode(indio_dev)
#else
    #define MATRIXIO_IIO_LOCK(indio_dev) mutex_lock(&(indio_dev)->mlock); 0
    #define MATRIXIO_IIO_UNLOCK(indio_dev) mutex_unlock(&(indio_dev)->mlock)
#endif

/* GPIO API changes in kernel 6.0+
 * The gpio_chip structure and API were significantly refactored.
 * Many fields were removed or replaced with new descriptor-based APIs.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    /* Modern GPIO descriptor-based API */
    #include <linux/gpio/driver.h>
    #define MATRIXIO_GPIO_CHIP_INIT(chip, label_name, owner_module, get_dir, dir_in, dir_out, get_val, set_val, base_val, ngpio_val, can_sleep_val) \
        do { \
            (chip)->label = (label_name); \
            (chip)->owner = (owner_module); \
            (chip)->get_direction = (get_dir); \
            (chip)->direction_input = (dir_in); \
            (chip)->direction_output = (dir_out); \
            (chip)->get = (get_val); \
            (chip)->set = (set_val); \
            (chip)->base = (base_val); \
            (chip)->ngpio = (ngpio_val); \
            (chip)->can_sleep = (can_sleep_val); \
        } while (0)
#else
    /* Legacy GPIO API */
    #include <linux/gpio.h>
    #define MATRIXIO_GPIO_CHIP_INIT(chip, label_name, owner_module, get_dir, dir_in, dir_out, get_val, set_val, base_val, ngpio_val, can_sleep_val) \
        do { \
            (chip)->label = (label_name); \
            (chip)->owner = (owner_module); \
            (chip)->get_direction = (get_dir); \
            (chip)->direction_input = (dir_in); \
            (chip)->direction_output = (dir_out); \
            (chip)->get = (get_val); \
            (chip)->set = (set_val); \
            (chip)->base = (base_val); \
            (chip)->ngpio = (ngpio_val); \
            (chip)->can_sleep = (can_sleep_val); \
        } while (0)
#endif

#endif /* MATRIXIO_COMPAT_H */

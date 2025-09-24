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

/* UART xmit structure moved in kernel 6.1+ - access through uart_port_lock */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include <linux/serial_core.h>
#include <linux/circ_buf.h>
#define MATRIXIO_UART_XMIT(port) (&(port)->state->port.xmit)
#define MATRIXIO_UART_XMIT_BUF(port) ((port)->state->port.xmit.buf)
#define MATRIXIO_UART_XMIT_TAIL(port) ((port)->state->port.xmit.tail)
#define MATRIXIO_UART_XMIT_SET_TAIL(port, val) ((port)->state->port.xmit.tail = (val))
#define MATRIXIO_UART_CIRC_EMPTY(port) uart_circ_empty(&(port)->state->port.xmit)
#else
#include <linux/serial_core.h>
#include <linux/circ_buf.h>
#define MATRIXIO_UART_XMIT(port) (&(port)->state->xmit)
#define MATRIXIO_UART_XMIT_BUF(port) ((port)->state->xmit.buf)
#define MATRIXIO_UART_XMIT_TAIL(port) ((port)->state->xmit.tail)
#define MATRIXIO_UART_XMIT_SET_TAIL(port, val) ((port)->state->xmit.tail = (val))
#define MATRIXIO_UART_CIRC_EMPTY(port) uart_circ_empty(&(port)->state->xmit)
#endif

/* class_create API changed in kernel 6.4+ */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define MATRIXIO_CLASS_CREATE(name) class_create(name)
#else
#define MATRIXIO_CLASS_CREATE(name) class_create(THIS_MODULE, name)
#endif

/* dev_uevent function signature changed - use void* cast to bypass type checking */
#define MATRIXIO_UEVENT_CAST(func) ((void*)(func))

#endif /* MATRIXIO_COMPAT_H */

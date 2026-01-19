/**
 * \file            nx_uart_kconfig.h
 * \brief           UART Kconfig instance definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * This file is auto-generated from Kconfig configuration.
 * Do not modify manually.
 */

#ifndef NEXUS_HAL_NX_UART_KCONFIG_H
#define NEXUS_HAL_NX_UART_KCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* UART Instance Definition Macros                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * UART instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_UART(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0) MY_REGISTER_FUNC(1) MY_REGISTER_FUNC(2)
 */
#define NX_DEFINE_INSTANCE_NX_UART(fn)                                         \
    _NX_UART_INSTANCE_0(fn)                                                    \
    _NX_UART_INSTANCE_1(fn)                                                    \
    _NX_UART_INSTANCE_2(fn)

/*---------------------------------------------------------------------------*/
/* UART0 Instance                                                           */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_UART_0
#define _NX_UART_INSTANCE_0(fn) fn(0)
#define CONFIG_UART0_ENABLED    1
#else
#define _NX_UART_INSTANCE_0(fn)
#define CONFIG_UART0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* UART1 Instance                                                           */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_UART_1
#define _NX_UART_INSTANCE_1(fn) fn(1)
#define CONFIG_UART1_ENABLED    1
#else
#define _NX_UART_INSTANCE_1(fn)
#define CONFIG_UART1_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* UART2 Instance                                                           */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_UART_2
#define _NX_UART_INSTANCE_2(fn) fn(2)
#define CONFIG_UART2_ENABLED    1
#else
#define _NX_UART_INSTANCE_2(fn)
#define CONFIG_UART2_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* UART Configuration Access Macros                                         */
/*---------------------------------------------------------------------------*/

/* UART0 Configuration */
#ifdef CONFIG_INSTANCE_NX_UART_0
#ifndef CONFIG_UART0_BAUDRATE
#define CONFIG_UART0_BAUDRATE 115200
#endif
#ifndef CONFIG_UART0_DATA_BITS
#define CONFIG_UART0_DATA_BITS 8
#endif
#ifndef CONFIG_UART0_STOP_BITS
#define CONFIG_UART0_STOP_BITS 1
#endif
#ifndef CONFIG_UART0_PARITY_VALUE
#define CONFIG_UART0_PARITY_VALUE 0
#endif
#ifndef CONFIG_UART0_TX_BUFFER_SIZE
#define CONFIG_UART0_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_UART0_RX_BUFFER_SIZE
#define CONFIG_UART0_RX_BUFFER_SIZE 256
#endif
#endif /* CONFIG_INSTANCE_NX_UART_0 */

/* UART1 Configuration */
#ifdef CONFIG_INSTANCE_NX_UART_1
#ifndef CONFIG_UART1_BAUDRATE
#define CONFIG_UART1_BAUDRATE 115200
#endif
#ifndef CONFIG_UART1_DATA_BITS
#define CONFIG_UART1_DATA_BITS 8
#endif
#ifndef CONFIG_UART1_STOP_BITS
#define CONFIG_UART1_STOP_BITS 1
#endif
#ifndef CONFIG_UART1_PARITY_VALUE
#define CONFIG_UART1_PARITY_VALUE 0
#endif
#ifndef CONFIG_UART1_TX_BUFFER_SIZE
#define CONFIG_UART1_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_UART1_RX_BUFFER_SIZE
#define CONFIG_UART1_RX_BUFFER_SIZE 256
#endif
#endif /* CONFIG_INSTANCE_NX_UART_1 */

/* UART2 Configuration */
#ifdef CONFIG_INSTANCE_NX_UART_2
#ifndef CONFIG_UART2_BAUDRATE
#define CONFIG_UART2_BAUDRATE 115200
#endif
#ifndef CONFIG_UART2_DATA_BITS
#define CONFIG_UART2_DATA_BITS 8
#endif
#ifndef CONFIG_UART2_STOP_BITS
#define CONFIG_UART2_STOP_BITS 1
#endif
#ifndef CONFIG_UART2_PARITY_VALUE
#define CONFIG_UART2_PARITY_VALUE 0
#endif
#ifndef CONFIG_UART2_TX_BUFFER_SIZE
#define CONFIG_UART2_TX_BUFFER_SIZE 256
#endif
#ifndef CONFIG_UART2_RX_BUFFER_SIZE
#define CONFIG_UART2_RX_BUFFER_SIZE 256
#endif
#endif /* CONFIG_INSTANCE_NX_UART_2 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_HAL_NX_UART_KCONFIG_H */

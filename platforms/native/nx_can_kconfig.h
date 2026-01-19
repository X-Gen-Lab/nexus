/**
 * \file            nx_can_kconfig.h
 * \brief           CAN Kconfig instance definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * This file is auto-generated from Kconfig configuration.
 * Do not modify manually.
 */

#ifndef NEXUS_HAL_NX_CAN_KCONFIG_H
#define NEXUS_HAL_NX_CAN_KCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* CAN Instance Definition Macros                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           CAN instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * CAN instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_CAN(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0) MY_REGISTER_FUNC(1)
 */
#define NX_DEFINE_INSTANCE_NX_CAN(fn)                                          \
    _NX_CAN_INSTANCE_0(fn)                                                     \
    _NX_CAN_INSTANCE_1(fn)

/*---------------------------------------------------------------------------*/
/* CAN0 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_CAN_0
#define _NX_CAN_INSTANCE_0(fn) fn(0)
#define CONFIG_CAN0_ENABLED    1
#else
#define _NX_CAN_INSTANCE_0(fn)
#define CONFIG_CAN0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* CAN1 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_CAN_1
#define _NX_CAN_INSTANCE_1(fn) fn(1)
#define CONFIG_CAN1_ENABLED    1
#else
#define _NX_CAN_INSTANCE_1(fn)
#define CONFIG_CAN1_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* CAN Configuration Access Macros                                          */
/*---------------------------------------------------------------------------*/

/* CAN0 Configuration */
#ifdef CONFIG_INSTANCE_NX_CAN_0
#ifndef CONFIG_CAN0_BAUDRATE
#define CONFIG_CAN0_BAUDRATE 500000
#endif
#ifndef CONFIG_CAN0_MODE_VALUE
#define CONFIG_CAN0_MODE_VALUE 0
#endif
#ifndef CONFIG_CAN0_PRESCALER
#define CONFIG_CAN0_PRESCALER 6
#endif
#ifndef CONFIG_CAN0_TIME_SEG1
#define CONFIG_CAN0_TIME_SEG1 13
#endif
#ifndef CONFIG_CAN0_TIME_SEG2
#define CONFIG_CAN0_TIME_SEG2 2
#endif
#ifndef CONFIG_CAN0_SJW
#define CONFIG_CAN0_SJW 1
#endif
#ifndef CONFIG_CAN0_TX_FIFO_SIZE
#define CONFIG_CAN0_TX_FIFO_SIZE 8
#endif
#ifndef CONFIG_CAN0_RX_FIFO_SIZE
#define CONFIG_CAN0_RX_FIFO_SIZE 16
#endif
#endif /* CONFIG_INSTANCE_NX_CAN_0 */

/* CAN1 Configuration */
#ifdef CONFIG_INSTANCE_NX_CAN_1
#ifndef CONFIG_CAN1_BAUDRATE
#define CONFIG_CAN1_BAUDRATE 500000
#endif
#ifndef CONFIG_CAN1_MODE_VALUE
#define CONFIG_CAN1_MODE_VALUE 0
#endif
#ifndef CONFIG_CAN1_PRESCALER
#define CONFIG_CAN1_PRESCALER 6
#endif
#ifndef CONFIG_CAN1_TIME_SEG1
#define CONFIG_CAN1_TIME_SEG1 13
#endif
#ifndef CONFIG_CAN1_TIME_SEG2
#define CONFIG_CAN1_TIME_SEG2 2
#endif
#ifndef CONFIG_CAN1_SJW
#define CONFIG_CAN1_SJW 1
#endif
#ifndef CONFIG_CAN1_TX_FIFO_SIZE
#define CONFIG_CAN1_TX_FIFO_SIZE 8
#endif
#ifndef CONFIG_CAN1_RX_FIFO_SIZE
#define CONFIG_CAN1_RX_FIFO_SIZE 16
#endif
#endif /* CONFIG_INSTANCE_NX_CAN_1 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_HAL_NX_CAN_KCONFIG_H */

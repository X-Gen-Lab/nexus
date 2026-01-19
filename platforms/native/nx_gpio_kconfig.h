/**
 * \file            nx_gpio_kconfig.h
 * \brief           GPIO Kconfig instance definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * This file is auto-generated from Kconfig configuration.
 * Do not modify manually.
 */

#ifndef NEXUS_HAL_NX_GPIO_KCONFIG_H
#define NEXUS_HAL_NX_GPIO_KCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* GPIO Instance Definition Macros                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * GPIO pin. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_GPIO(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(A, 0) MY_REGISTER_FUNC(B, 0) ...
 */
#define NX_DEFINE_INSTANCE_NX_GPIO(fn)                                         \
    _NX_GPIO_INSTANCE_A(fn)                                                    \
    _NX_GPIO_INSTANCE_B(fn)                                                    \
    _NX_GPIO_INSTANCE_C(fn)

/*---------------------------------------------------------------------------*/
/* GPIOA Instances                                                          */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_GPIOA
#define _NX_GPIO_INSTANCE_A(fn) _NX_GPIOA_PIN0(fn)
#else
#define _NX_GPIO_INSTANCE_A(fn)
#endif

#ifdef CONFIG_INSTANCE_NX_GPIOA_PIN0
#define _NX_GPIOA_PIN0(fn)        fn(A, 0)
#define CONFIG_GPIOA_PIN0_ENABLED 1
#else
#define _NX_GPIOA_PIN0(fn)
#define CONFIG_GPIOA_PIN0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* GPIOB Instances                                                          */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_GPIOB
#define _NX_GPIO_INSTANCE_B(fn) _NX_GPIOB_PIN0(fn)
#else
#define _NX_GPIO_INSTANCE_B(fn)
#endif

#ifdef CONFIG_INSTANCE_NX_GPIOB_PIN0
#define _NX_GPIOB_PIN0(fn)        fn(B, 0)
#define CONFIG_GPIOB_PIN0_ENABLED 1
#else
#define _NX_GPIOB_PIN0(fn)
#define CONFIG_GPIOB_PIN0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* GPIOC Instances                                                          */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_GPIOC
#define _NX_GPIO_INSTANCE_C(fn) _NX_GPIOC_PIN0(fn)
#else
#define _NX_GPIO_INSTANCE_C(fn)
#endif

#ifdef CONFIG_INSTANCE_NX_GPIOC_PIN0
#define _NX_GPIOC_PIN0(fn)        fn(C, 0)
#define CONFIG_GPIOC_PIN0_ENABLED 1
#else
#define _NX_GPIOC_PIN0(fn)
#define CONFIG_GPIOC_PIN0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* GPIO Configuration Access Macros                                         */
/*---------------------------------------------------------------------------*/

/* GPIOA PIN0 Configuration */
#ifdef CONFIG_INSTANCE_NX_GPIOA_PIN0
#ifndef CONFIG_GPIOA_PIN0_WR_MODE
#define CONFIG_GPIOA_PIN0_WR_MODE 2
#endif
#ifndef CONFIG_GPIOA_PIN0_MODE
#define CONFIG_GPIOA_PIN0_MODE 0x01
#endif
#ifndef CONFIG_GPIOA_PIN0_PULL_VALUE
#define CONFIG_GPIOA_PIN0_PULL_VALUE 0x00
#endif
#ifndef CONFIG_GPIOA_PIN0_SPEED_VALUE
#define CONFIG_GPIOA_PIN0_SPEED_VALUE 0x01
#endif
#ifndef CONFIG_GPIOA_PIN0_OUTPUT_VALUE
#define CONFIG_GPIOA_PIN0_OUTPUT_VALUE 0
#endif
#endif /* CONFIG_INSTANCE_NX_GPIOA_PIN0 */

/* GPIOB PIN0 Configuration */
#ifdef CONFIG_INSTANCE_NX_GPIOB_PIN0
#ifndef CONFIG_GPIOB_PIN0_WR_MODE
#define CONFIG_GPIOB_PIN0_WR_MODE 2
#endif
#ifndef CONFIG_GPIOB_PIN0_MODE
#define CONFIG_GPIOB_PIN0_MODE 0x01
#endif
#ifndef CONFIG_GPIOB_PIN0_PULL_VALUE
#define CONFIG_GPIOB_PIN0_PULL_VALUE 0x00
#endif
#ifndef CONFIG_GPIOB_PIN0_SPEED_VALUE
#define CONFIG_GPIOB_PIN0_SPEED_VALUE 0x01
#endif
#ifndef CONFIG_GPIOB_PIN0_OUTPUT_VALUE
#define CONFIG_GPIOB_PIN0_OUTPUT_VALUE 0
#endif
#endif /* CONFIG_INSTANCE_NX_GPIOB_PIN0 */

/* GPIOC PIN0 Configuration */
#ifdef CONFIG_INSTANCE_NX_GPIOC_PIN0
#ifndef CONFIG_GPIOC_PIN0_WR_MODE
#define CONFIG_GPIOC_PIN0_WR_MODE 2
#endif
#ifndef CONFIG_GPIOC_PIN0_MODE
#define CONFIG_GPIOC_PIN0_MODE 0x01
#endif
#ifndef CONFIG_GPIOC_PIN0_PULL_VALUE
#define CONFIG_GPIOC_PIN0_PULL_VALUE 0x00
#endif
#ifndef CONFIG_GPIOC_PIN0_SPEED_VALUE
#define CONFIG_GPIOC_PIN0_SPEED_VALUE 0x01
#endif
#ifndef CONFIG_GPIOC_PIN0_OUTPUT_VALUE
#define CONFIG_GPIOC_PIN0_OUTPUT_VALUE 0
#endif
#endif /* CONFIG_INSTANCE_NX_GPIOC_PIN0 */

/*---------------------------------------------------------------------------*/
/* Helper Macros                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Convert port letter to port number
 */
#define NX_GPIO_PORT_NUM(port)                                                 \
    ((port) == 'A'   ? 0                                                       \
     : (port) == 'B' ? 1                                                       \
     : (port) == 'C' ? 2                                                       \
     : (port) == 'D' ? 3                                                       \
     : (port) == 'E' ? 4                                                       \
     : (port) == 'F' ? 5                                                       \
     : (port) == 'G' ? 6                                                       \
     : (port) == 'H' ? 7                                                       \
                     : 0)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_HAL_NX_GPIO_KCONFIG_H */

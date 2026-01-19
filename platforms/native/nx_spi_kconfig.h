/**
 * \file            nx_spi_kconfig.h
 * \brief           SPI Kconfig instance definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * This file is auto-generated from Kconfig configuration.
 * Do not modify manually.
 */

#ifndef NEXUS_HAL_NX_SPI_KCONFIG_H
#define NEXUS_HAL_NX_SPI_KCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* SPI Instance Definition Macros                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * SPI instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_SPI(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0) MY_REGISTER_FUNC(1) MY_REGISTER_FUNC(2)
 */
#define NX_DEFINE_INSTANCE_NX_SPI(fn)                                          \
    _NX_SPI_INSTANCE_0(fn)                                                     \
    _NX_SPI_INSTANCE_1(fn)                                                     \
    _NX_SPI_INSTANCE_2(fn)

/*---------------------------------------------------------------------------*/
/* SPI0 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_SPI_0
#define _NX_SPI_INSTANCE_0(fn) fn(0)
#define CONFIG_SPI0_ENABLED    1
#else
#define _NX_SPI_INSTANCE_0(fn)
#define CONFIG_SPI0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* SPI1 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_SPI_1
#define _NX_SPI_INSTANCE_1(fn) fn(1)
#define CONFIG_SPI1_ENABLED    1
#else
#define _NX_SPI_INSTANCE_1(fn)
#define CONFIG_SPI1_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* SPI2 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_SPI_2
#define _NX_SPI_INSTANCE_2(fn) fn(2)
#define CONFIG_SPI2_ENABLED    1
#else
#define _NX_SPI_INSTANCE_2(fn)
#define CONFIG_SPI2_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* SPI Configuration Access Macros                                          */
/*---------------------------------------------------------------------------*/

/* SPI0 Configuration */
#ifdef CONFIG_INSTANCE_NX_SPI_0
#ifndef CONFIG_SPI0_CLOCK_FREQ
#define CONFIG_SPI0_CLOCK_FREQ 1000000
#endif
#ifndef CONFIG_SPI0_MODE_VALUE
#define CONFIG_SPI0_MODE_VALUE 0
#endif
#ifndef CONFIG_SPI0_BIT_ORDER_VALUE
#define CONFIG_SPI0_BIT_ORDER_VALUE 0
#endif
#ifndef CONFIG_SPI0_MOSI_PIN
#define CONFIG_SPI0_MOSI_PIN -1
#endif
#ifndef CONFIG_SPI0_MISO_PIN
#define CONFIG_SPI0_MISO_PIN -1
#endif
#ifndef CONFIG_SPI0_SCK_PIN
#define CONFIG_SPI0_SCK_PIN -1
#endif
#endif /* CONFIG_INSTANCE_NX_SPI_0 */

/* SPI1 Configuration */
#ifdef CONFIG_INSTANCE_NX_SPI_1
#ifndef CONFIG_SPI1_CLOCK_FREQ
#define CONFIG_SPI1_CLOCK_FREQ 1000000
#endif
#ifndef CONFIG_SPI1_MODE_VALUE
#define CONFIG_SPI1_MODE_VALUE 0
#endif
#ifndef CONFIG_SPI1_BIT_ORDER_VALUE
#define CONFIG_SPI1_BIT_ORDER_VALUE 0
#endif
#ifndef CONFIG_SPI1_MOSI_PIN
#define CONFIG_SPI1_MOSI_PIN -1
#endif
#ifndef CONFIG_SPI1_MISO_PIN
#define CONFIG_SPI1_MISO_PIN -1
#endif
#ifndef CONFIG_SPI1_SCK_PIN
#define CONFIG_SPI1_SCK_PIN -1
#endif
#endif /* CONFIG_INSTANCE_NX_SPI_1 */

/* SPI2 Configuration */
#ifdef CONFIG_INSTANCE_NX_SPI_2
#ifndef CONFIG_SPI2_CLOCK_FREQ
#define CONFIG_SPI2_CLOCK_FREQ 1000000
#endif
#ifndef CONFIG_SPI2_MODE_VALUE
#define CONFIG_SPI2_MODE_VALUE 0
#endif
#ifndef CONFIG_SPI2_BIT_ORDER_VALUE
#define CONFIG_SPI2_BIT_ORDER_VALUE 0
#endif
#ifndef CONFIG_SPI2_MOSI_PIN
#define CONFIG_SPI2_MOSI_PIN -1
#endif
#ifndef CONFIG_SPI2_MISO_PIN
#define CONFIG_SPI2_MISO_PIN -1
#endif
#ifndef CONFIG_SPI2_SCK_PIN
#define CONFIG_SPI2_SCK_PIN -1
#endif
#endif /* CONFIG_INSTANCE_NX_SPI_2 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_HAL_NX_SPI_KCONFIG_H */

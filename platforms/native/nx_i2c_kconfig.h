/**
 * \file            nx_i2c_kconfig.h
 * \brief           I2C Kconfig instance definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * This file is auto-generated from Kconfig configuration.
 * Do not modify manually.
 */

#ifndef NEXUS_HAL_NX_I2C_KCONFIG_H
#define NEXUS_HAL_NX_I2C_KCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* I2C Instance Definition Macros                                           */
/*---------------------------------------------------------------------------*/

/**
 * \brief           I2C instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * I2C instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_I2C(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0) MY_REGISTER_FUNC(1) MY_REGISTER_FUNC(2)
 */
#define NX_DEFINE_INSTANCE_NX_I2C(fn)                                          \
    _NX_I2C_INSTANCE_0(fn)                                                     \
    _NX_I2C_INSTANCE_1(fn)                                                     \
    _NX_I2C_INSTANCE_2(fn)

/*---------------------------------------------------------------------------*/
/* I2C0 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_I2C_0
#define _NX_I2C_INSTANCE_0(fn) fn(0)
#define CONFIG_I2C0_ENABLED    1
#else
#define _NX_I2C_INSTANCE_0(fn)
#define CONFIG_I2C0_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* I2C1 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_I2C_1
#define _NX_I2C_INSTANCE_1(fn) fn(1)
#define CONFIG_I2C1_ENABLED    1
#else
#define _NX_I2C_INSTANCE_1(fn)
#define CONFIG_I2C1_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* I2C2 Instance                                                            */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_INSTANCE_NX_I2C_2
#define _NX_I2C_INSTANCE_2(fn) fn(2)
#define CONFIG_I2C2_ENABLED    1
#else
#define _NX_I2C_INSTANCE_2(fn)
#define CONFIG_I2C2_ENABLED 0
#endif

/*---------------------------------------------------------------------------*/
/* I2C Configuration Access Macros                                          */
/*---------------------------------------------------------------------------*/

/* I2C0 Configuration */
#ifdef CONFIG_INSTANCE_NX_I2C_0
#ifndef CONFIG_I2C0_SPEED_VALUE
#define CONFIG_I2C0_SPEED_VALUE 100000
#endif
#ifndef CONFIG_I2C0_SDA_PIN
#define CONFIG_I2C0_SDA_PIN -1
#endif
#ifndef CONFIG_I2C0_SCL_PIN
#define CONFIG_I2C0_SCL_PIN -1
#endif
#ifndef CONFIG_I2C0_TIMEOUT_MS
#define CONFIG_I2C0_TIMEOUT_MS 1000
#endif
#endif /* CONFIG_INSTANCE_NX_I2C_0 */

/* I2C1 Configuration */
#ifdef CONFIG_INSTANCE_NX_I2C_1
#ifndef CONFIG_I2C1_SPEED_VALUE
#define CONFIG_I2C1_SPEED_VALUE 100000
#endif
#ifndef CONFIG_I2C1_SDA_PIN
#define CONFIG_I2C1_SDA_PIN -1
#endif
#ifndef CONFIG_I2C1_SCL_PIN
#define CONFIG_I2C1_SCL_PIN -1
#endif
#ifndef CONFIG_I2C1_TIMEOUT_MS
#define CONFIG_I2C1_TIMEOUT_MS 1000
#endif
#endif /* CONFIG_INSTANCE_NX_I2C_1 */

/* I2C2 Configuration */
#ifdef CONFIG_INSTANCE_NX_I2C_2
#ifndef CONFIG_I2C2_SPEED_VALUE
#define CONFIG_I2C2_SPEED_VALUE 100000
#endif
#ifndef CONFIG_I2C2_SDA_PIN
#define CONFIG_I2C2_SDA_PIN -1
#endif
#ifndef CONFIG_I2C2_SCL_PIN
#define CONFIG_I2C2_SCL_PIN -1
#endif
#ifndef CONFIG_I2C2_TIMEOUT_MS
#define CONFIG_I2C2_TIMEOUT_MS 1000
#endif
#endif /* CONFIG_INSTANCE_NX_I2C_2 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_HAL_NX_I2C_KCONFIG_H */

/**
 * \file            stm32f4_traits.h
 * \brief           Selected reviewed silicon group, without runtime discovery
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_TRAITS_H
#define NX_STM32F4_TRAITS_H

/* These are compile-time source selections. A documented cut is not an
 * observation, PCB qualification or permission for an unreviewed mode. */
#if (defined(STM32F405xx) + defined(STM32F407xx) + defined(STM32F415xx) +      \
     defined(STM32F417xx) + defined(STM32F411xE) + defined(STM32F446xx) +      \
     defined(STM32F479xx)) != 1
#error "STM32F4 requires exactly one reviewed SDK device selection"
#endif

#if defined(STM32F411xE)
#define NX_STM32_DEVICE_GROUP_ID   0x431U
#define NX_STM32_LAST_EXTERNAL_IRQ SPI5_IRQn
#define NX_STM32_SINGLE_CUT        1
#define NX_STM32_FLASH_MIN_BYTES   524288U
#define NX_STM32_FLASH_MAX_BYTES   524288U
#elif defined(STM32F446xx)
#define NX_STM32_DEVICE_GROUP_ID   0x421U
#define NX_STM32_LAST_EXTERNAL_IRQ FMPI2C1_ER_IRQn
#define NX_STM32_SINGLE_CUT        1
#define NX_STM32_FLASH_MIN_BYTES   262144U
#define NX_STM32_FLASH_MAX_BYTES   524288U
#elif defined(STM32F479xx)
#define NX_STM32_DEVICE_GROUP_ID   0x434U
#define NX_STM32_LAST_EXTERNAL_IRQ DSI_IRQn
#define NX_STM32_SINGLE_CUT        1
#define NX_STM32_FLASH_MIN_BYTES   1048576U
#define NX_STM32_FLASH_MAX_BYTES   2097152U
#else
#define NX_STM32_DEVICE_GROUP_ID   0x413U
#define NX_STM32_LAST_EXTERNAL_IRQ FPU_IRQn
#define NX_STM32_SINGLE_CUT        0
#define NX_STM32_FLASH_MIN_BYTES   524288U
#define NX_STM32_FLASH_MAX_BYTES   1048576U
#endif

#define NX_STM32_EXTERNAL_IRQ_COUNT ((unsigned)NX_STM32_LAST_EXTERNAL_IRQ + 1U)

#endif /* NX_STM32F4_TRAITS_H */

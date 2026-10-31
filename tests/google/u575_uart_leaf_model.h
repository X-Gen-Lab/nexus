/**
 * \file            u575_uart_leaf_model.h
 * \brief           Production UART word MMIO with external IRQ and clock facts
 * \author          Nexus Team
 */
#ifndef NX_U575_UART_LEAF_MODEL_H
#define NX_U575_UART_LEAF_MODEL_H
#define NX_U575_UART_DEFAULT_PORT 1
#include "u575_model.h"

#include "stm32u575_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_u575_uart_official_write(USART_TypeDef* registers, uint8_t value);
uint8_t nx_u575_uart_official_read(USART_TypeDef* registers);
extern USART_TypeDef g_u575_uart_leaf_decoy[2];
#ifdef __cplusplus
}
#endif
/* No model replaces a data access or its width. Tests supply external flags. */
#undef NX_U575_READ
#undef NX_U575_WRITE
#define NX_U575_READ(reg)         (*(reg))
#define NX_U575_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#undef USART1
#undef USART2
#define USART1 (&g_u575_uart_leaf_decoy[0])
#define USART2 (&g_u575_uart_leaf_decoy[1])
#endif

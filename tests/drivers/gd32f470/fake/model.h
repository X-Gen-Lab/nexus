#ifndef NEXUS_TEST_GD32_MODEL_H
#define NEXUS_TEST_GD32_MODEL_H
#include "gd32f4xx.h"
#include <stdbool.h>
#include <stddef.h>
extern uint32_t fake_mask,fake_isr,fake_millis,fake_clock_step,fake_usart_interrupts;
extern uint32_t fake_reset_count,fake_spi_flags,fake_spi_writes,fake_uart_writes;
extern unsigned fake_board_safe_inits;
extern uint8_t fake_uart_rx;
extern bool fake_de,fake_cs,fake_uart_shift,fake_spi_shift,fake_spi_stall,fake_spi_hold_busy;
extern void (*fake_spi_hook)(void);
extern spi_parameter_struct fake_spi_config;
extern uint32_t fake_timer_count;
extern bool fake_timer_pending;
extern timer_parameter_struct fake_timer_config;
#endif

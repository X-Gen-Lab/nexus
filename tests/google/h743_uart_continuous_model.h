/**
 * \file            h743_uart_continuous_model.h
 * \brief           External DBM target, UART shift and cache visibility model
 * \author          Nexus Team
 */
#ifndef NX_H743_UART_CONTINUOUS_MODEL_H
#define NX_H743_UART_CONTINUOUS_MODEL_H
#include "h743_model.h"
#include "nexus/arch/cache.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_h743_uart_continuous_model_reset(void);
typedef struct {
    USART_TypeDef uart;
    DMA_TypeDef dma;
    DMA_Stream_TypeDef rx;
    DMAMUX_Channel_TypeDef mux;
    DMA_Stream_TypeDef tx;
    DMAMUX_Channel_TypeDef tx_mux;
    uint8_t bus[0x80000];
    uint8_t input[65536];
    size_t input_count;
    size_t input_index;
    uint32_t initial;
    uint32_t frame_cycles;
    uint32_t peer_frame_cycles;
    uintptr_t pending_address;
    uint8_t pending_byte;
    bool pending;
    bool stopping;
    bool hold_commit;
    bool hold_en;
    bool refuse_cache;
    bool race_target;
    unsigned cache_calls;
    unsigned cache_errors;
    unsigned target_errors;
    unsigned forbidden;
    unsigned enabled_transitions;
    unsigned frames;
    unsigned commits;
    unsigned exception;
    size_t tx_initial;
    size_t tx_count;
    unsigned tx_frame_cycles;
    uint8_t output[65536];
    uint8_t tx_byte;
    bool tx_pending;
    bool hold_tx_en;
    bool hold_tx_wire;
    unsigned clean_calls;
    bool tainted;
    unsigned accesses_after_isolation;
    void (*write_observer)(volatile uint32_t* reg, uint32_t value);
    volatile uint32_t* refuse_write;
    void (*cache_observer)(void);
    void (*clean_observer)(void);
} nx_h743_uart_continuous_model_t;
extern nx_h743_uart_continuous_model_t g_h743_uart_continuous_model;
uint32_t nx_h743_uart_continuous_model_read(volatile uint32_t* reg);
void nx_h743_uart_continuous_model_write(volatile uint32_t* reg,
                                         uint32_t value);
void nx_h743_uart_continuous_model_step(void);
void nx_h743_uart_continuous_model_feed(const uint8_t* data, size_t bytes);
bool nx_h743_uart_continuous_model_level(int irq);
nx_arch_result_t nx_h743_uart_continuous_model_invalidate(uintptr_t address,
                                                          size_t bytes);
nx_arch_result_t nx_h743_uart_continuous_model_clean(uintptr_t address,
                                                     size_t bytes);
#ifdef __cplusplus
}
#endif
#undef NX_H743_READ
#undef NX_H743_WRITE
#define NX_H743_READ(p)     nx_h743_uart_continuous_model_read(p)
#define NX_H743_WRITE(p, v) nx_h743_uart_continuous_model_write((p), (v))
#define NX_H743_DMA_INVALIDATE(p, n)                                           \
    nx_h743_uart_continuous_model_invalidate((p), (n))
#define NX_H743_DMA_CLEAN(p, n)  nx_h743_uart_continuous_model_clean((p), (n))
#define NX_H743_IRQ_ENABLED(irq) g_h743_model.irq_enabled[(irq)]
#undef DMA1
#undef DMA1_Stream1
#undef DMA1_Stream0
#undef DMAMUX1_Channel1
#undef DMAMUX1_Channel0
#undef USART3
#define DMA1             (&g_h743_uart_continuous_model.dma)
#define DMA1_Stream1     (&g_h743_uart_continuous_model.rx)
#define DMA1_Stream0     (&g_h743_uart_continuous_model.tx)
#define DMAMUX1_Channel1 (&g_h743_uart_continuous_model.mux)
#define DMAMUX1_Channel0 (&g_h743_uart_continuous_model.tx_mux)
#define USART3           (&g_h743_uart_continuous_model.uart)
#undef RCC
#undef FLASH
#undef PWR
#undef TIM2
#undef GPIOD
#define RCC   (&g_h743_model.rcc)
#define FLASH (&g_h743_model.flash)
#define PWR   (&g_h743_model.power)
#define TIM2  (&g_h743_model.timer)
#define GPIOD (&g_h743_model.gpio_d)
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_h743_uart_continuous_model_write(&(reg), (uint32_t)(value))
#define nx_arch_exception_number() (g_h743_uart_continuous_model.exception)
#define nx_arch_in_isr()           (g_h743_uart_continuous_model.exception != 0U)
#endif

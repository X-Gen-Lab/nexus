/**
 * \file            u575_model.h
 * \brief           U575 external MMIO and NVIC boundaries for real providers
 * \author          Nexus Team
 */
#ifndef NX_U575_MODEL_H
#define NX_U575_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32u575xx.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RCC_TypeDef rcc;
    PWR_TypeDef power;
    FLASH_TypeDef flash;
    TIM_TypeDef timer;
    ICACHE_TypeDef icache;
    DCACHE_TypeDef dcache;
    GPIO_TypeDef gpio;
    GPIO_TypeDef gpioa;
    GPIO_TypeDef extra_gpio[6];
    struct {
        uint32_t CPACR;
        uint32_t VTOR;
    } cpu;
    USART_TypeDef* uart;
    USART_TypeDef* uart2;
    IWDG_TypeDef* watchdog;
    bool watchdog_stuck_after_start;
    void (*watchdog_start_observer)(void* context);
    void* watchdog_start_context;
    volatile uint32_t* ignore_write;
    bool irq_enabled[126];
    bool irq_pending[126];
    bool irq_active[126];
    uint8_t priority[126];
    uint8_t group;
    uint16_t flash_kib;
    bool privileged;
    bool hold_uart_on;
    bool hold_uart_off;
    bool hold_irq_on;
    bool hold_irq_off;
    bool wrap_after_count;
    bool hide_unclocked_timer;
    bool refuse_timer_quiet_clear;
    bool timer_pending_on_dier;
    bool hold_lsi_off;
    uint32_t timer_trgo_pulses;
    uint32_t timer_ug_mms;
    uint32_t reads;
    uint32_t writes;
    uint32_t gpio_writes;
    uint32_t transmitted;
    void (*critical_observer)(void* context, bool entering);
    void* critical_context;
    bool observing_critical;
    void (*before_irq_save)(void);
} nx_u575_model_t;
extern nx_u575_model_t g_u575_model;
void nx_u575_model_reset(void);
bool nx_u575_model_privileged(void);
uint32_t nx_u575_model_read(volatile uint32_t* reg);
void nx_u575_model_write(volatile uint32_t* reg, uint32_t value);
void nx_u575_model_uart_poll(void* port);
nx_result_t nx_u575_model_irq_control(int irq, bool enabled, uint8_t priority);
nx_arch_irq_state_t nx_u575_model_irq_save(void);
void nx_u575_model_irq_restore(nx_arch_irq_state_t previous);
#define nx_arch_irq_save()         nx_u575_model_irq_save()
#define nx_arch_irq_restore(saved) nx_u575_model_irq_restore(saved)
#define nx_arch_is_privileged()    nx_u575_model_privileged()
#define NX_U575_MODEL              1
#define NX_U575_IRQ_AVAILABLE(irq)                                             \
    (!g_u575_model.irq_enabled[(irq)] && !g_u575_model.irq_pending[(irq)] &&   \
     !g_u575_model.irq_active[(irq)])
#define NX_U575_GPIOA             (&g_u575_model.gpioa)
#define NX_U575_GPIOG             (&g_u575_model.extra_gpio[4])
#define NX_U575_PWR               (&g_u575_model.power)
#define NX_U575_RCC               (&g_u575_model.rcc)
#define NX_U575_CPU_SCB           (&g_u575_model.cpu)
#define NX_U575_CPU_GROUP(value)  (g_u575_model.group = (value))
#define NX_U575_READ(reg)         nx_u575_model_read(reg)
#define NX_U575_WRITE(reg, value) nx_u575_model_write((reg), (value))
#define NX_U575_UART_POLL(port)   nx_u575_model_uart_poll(port)
#define NX_U575_IRQ_CONTROL(irq, enabled, priority)                            \
    nx_u575_model_irq_control((irq), (enabled), (priority))
#define NX_U575_IRQ_PRIORITY(irq) g_u575_model.priority[(irq)]
#define NX_U575_IRQ_GROUP()       g_u575_model.group
#define NX_U575_FLASH_KIB()       g_u575_model.flash_kib
#define NX_U575_FLASH_OPTIONS()   g_u575_model.flash.OPTR
#ifdef __cplusplus
}
#endif
#endif

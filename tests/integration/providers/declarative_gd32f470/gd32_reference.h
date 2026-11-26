/**
 * \file            gd32_reference.h
 * \brief           Private GPIO and UART owners for a GD32 software reference
 * \author          Nexus Team
 */
#ifndef NX_GD32_REFERENCE_H
#define NX_GD32_REFERENCE_H

#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_usart.h"
#include "nexus/core/platform_lifecycle.h"
#include "nexus/io/gpio.h"
#include "nexus/io/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t registers;
    uint32_t mask;
    uint32_t initial;
} nx_gd32_gpio_config_t;

typedef struct {
    const nx_gd32_gpio_config_t* config;
    uint32_t saved_mode;
    uint32_t saved_type;
    uint32_t saved_speed;
    uint32_t saved_pull;
    uint32_t saved_output;
} nx_gd32_gpio_state_t;

typedef struct {
    uint32_t registers;
    uint32_t baud;
    nx_uart_rx_event_t* rx_storage;
    size_t rx_capacity;
    uint8_t priority;
} nx_gd32_uart_config_t;

typedef struct {
    const nx_gd32_uart_config_t* config;
    nx_uart_tx_request_t* volatile active;
    volatile size_t transferred;
    volatile size_t head;
    size_t tail;
    volatile size_t queued;
    nx_result_t terminal_result;
    volatile bool terminal;
    volatile bool draining;
    volatile bool lost;
    bool closing;
    uint8_t saved_irq_priority;
} nx_gd32_uart_state_t;

extern const nx_gpio_ops_t nx_gd32_gpio_ops;
extern const nx_uart_ops_t nx_gd32_uart_ops;

/** \brief Validate immutable GPIO metadata before Platform effects.
 * \param[in,out] state: Zero initialized, unowned static storage.
 * \param[in] config: Immutable bank, authorization mask and initial level.
 * \return SUCCESS or rejection without writes or retained references.
 * \note Privileged unmasked task; prepare rechecks shared clock availability.
 */
nx_result_t nx_gd32_gpio_admit(nx_gd32_gpio_state_t* state,
                               const nx_gd32_gpio_config_t* config);
/** \brief Validate UART metadata and idle NVIC before Platform effects.
 * \param[in,out] state: Zero initialized, sole controller storage.
 * \param[in] config: Immutable controller, baud, RX storage and priority.
 * \return SUCCESS or rejection without writes or retained references.
 * \note Privileged unmasked task; prepare rechecks live hardware after gates.
 */
nx_result_t nx_gd32_uart_admit(nx_gd32_uart_state_t* state,
                               const nx_gd32_uart_config_t* config);

/**
 * \brief Admit a static GPIO output view after shared bank clocks are live.
 * \param[in,out] state: Zero initialized unowned storage, retained until close.
 * \param[in] config: Immutable authorized bank, mask and initial output.
 * \return SUCCESS or INVALID/STATE/BUSY before any change; IO retains the
 *         captured receipt for verified close after a hardware write failure.
 * \note Single privileged unmasked task owns prepare/close and serialized I/O.
 *       Shared bank clocks belong to the platform resource owner.
 */
nx_result_t nx_gd32_gpio_prepare(nx_gd32_gpio_state_t* state,
                                 const nx_gd32_gpio_config_t* config);
/**
 * \brief Restore only the GPIO fields captured by this static view.
 * \param[in,out] state: Same storage, callers and IRQ users already joined.
 * \return SUCCESS after restoration; BUSY/IO retains the receipt for retry.
 * \note Single privileged unmasked task; no hidden owner or allocation.
 */
nx_result_t nx_gd32_gpio_close(nx_gd32_gpio_state_t* state);
/**
 * \brief Start one USART0 8N1 RX-events and IRQ-TX software reference.
 * \param[in,out] state: Zero initialized sole controller owner.
 * \param[in] config: Static registers, baud and caller-provided event storage.
 * \return SUCCESS or rejection without a request/buffer borrow.
 * \note Platform already owns its GPIO/USART clocks, AF route and live clock.
 *       No scheduler is started. State/config/storage outlive all IRQ users.
 */
nx_result_t nx_gd32_uart_prepare(nx_gd32_uart_state_t* state,
                                 const nx_gd32_uart_config_t* config);
/**
 * \brief Close admission, drain TX, then withdraw RX and UART IRQ.
 * \param[in,out] state: Sole unmasked task owner, users joined before close.
 * \return SUCCESS, BUSY for an outstanding borrow, or INVALID/IO.
 * \note BUSY retains hardware and storage; cancel/deadline do not revoke loans.
 */
nx_result_t nx_gd32_uart_close(nx_gd32_uart_state_t* state);
/**
 * \brief Validate one transport's exact READY/QUEUED request without effects.
 * \param[in,out] context: Exact static UART state used by its interface.
 * \param[in] request: Live descriptor and unchanged payload, never retained.
 * \return Same rejection checks as direct or adapter-admitted transport start.
 * \note One unmasked task executor; no lock, hardware write or admission.
 */
nx_result_t nx_gd32_uart_validate(void* context,
                                  const nx_uart_tx_request_t* request);
/**
 * \brief Progress one bounded USART0 hardware fact, never settle a request.
 * \param[in,out] state: Same controller storage, live through IRQ withdrawal.
 * \note USART0 vector only. Task service handles deadlines and final
 * settlement.
 */
void nx_gd32_uart_irq(nx_gd32_uart_state_t* state);

/** \brief Query the explicitly owned monotonic timer domain. */
bool nx_gd32_system_ready(void);
/** \brief Start reset-only IRC16M/TIMER1 after all static admissions. */
nx_platform_start_result_t nx_gd32_system_start(void);
/** \brief Acquire only shared GPIO and UART route resources. */
nx_result_t nx_gd32_resources_acquire(void);
/** \brief Restore the selected route after its controllers have stopped. */
nx_result_t nx_gd32_resources_release(void);
/** \brief End the time domain after all old clock users have joined. */
nx_result_t nx_gd32_system_release(bool started);
/** \brief Fixed timebase vector; no other owner may acquire TIMER1. */
void TIMER1_IRQHandler(void);

#ifdef __cplusplus
}
#endif
#endif /* NX_GD32_REFERENCE_H */

/**
 * \file            stm32f407_spi_frames.h
 * \brief           Private F407 SPI frame owner and cold static binding
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_SPI_FRAMES_H
#define NX_STM32F407_SPI_FRAMES_H

#include "nexus/io/spi_frames.h"
#include "stm32f407_provider.h"
#include "stm32f407_spi_cold.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Complete readonly frame binding; no application protocol state. */
typedef struct {
    SPI_TypeDef* registers;
    RCC_TypeDef* rcc;
    GPIO_TypeDef* nss_gpio;
    GPIO_TypeDef* sck_gpio;
    uint32_t nss_mask;
    uint32_t sck_mask;
    uint32_t clock_hz;
    uint32_t maximum_hz;
    nx_spi_frames_config_t wire;
    IRQn_Type irq;
    uint8_t priority;
} nx_stm32_spi_frames_config_t;

struct nx_stm32_spi_frames_cold_config;
struct nx_stm32_spi_frames_cold;

/** \brief A single retained request; IRQ facts never release caller memory. */
typedef struct {
    const nx_stm32_spi_frames_config_t* config;
    const struct nx_stm32_spi_frames_cold_config* cold_config;
    struct nx_stm32_spi_frames_cold* cold_receipt;
    nx_spi_frames_request_t* active;
    uint64_t generation;
    uint64_t time_epoch;
    size_t tx_position;
    size_t rx_position;
    size_t completed;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    uint32_t control1;
    uint32_t control2;
    uint16_t divider;
    uint32_t receive_tail_delay_us;
    bool initialized;
    bool closing;
    bool fault;
    bool terminal_latched;
    bool receive_tail;
    bool crc_received;
    bool slave_last_disabled;
} nx_stm32_spi_frames_state_t;

/** \brief Exact ordered SCK/data/NSS pads and retained engine facts. */
typedef struct nx_stm32_spi_frames_cold_config {
    nx_stm32_spi_frames_config_t engine;
    const nx_stm32_spi_pin_config_t* pins;
    size_t pin_count;
} nx_stm32_spi_frames_cold_config_t;

/** \brief Actual effects survive every failed cold restoration. */
typedef struct nx_stm32_spi_frames_cold {
    uint32_t cr1;
    uint32_t cr2;
    uint32_t crcpr;
    uint32_t apb;
    uint32_t gpio_clocks;
    uint32_t gpio_mask;
    uint32_t priority;
    size_t captured;
    bool owned;
    bool registers_captured;
    bool vector_captured;
} nx_stm32_spi_frames_cold_t;

/** \brief Pure complete cold admission before shared Platform effects.
 * \param[in] state: Unbound engine, without an active request.
 * \param[in] config: Exact immutable controller, IRQ, pad and wire tuple.
 * \param[in] receipt: Empty retained-effects storage.
 * \return Success or a refusal retaining no new effect or reference.
 */
nx_result_t
nx_stm32_spi_frames_admit(const nx_stm32_spi_frames_state_t* state,
                          const nx_stm32_spi_frames_cold_config_t* config,
                          const nx_stm32_spi_frames_cold_t* receipt);
/** \brief Acquire a cold pad/clock/vector tuple with retryable cleanup.
 * \param[in,out] state: Stable zero storage through close and failed cleanup.
 * \param[in] config: Unchanged immutable facts through complete close.
 * \param[in,out] receipt: Stable effects storage, preserved on failure.
 * \return Success or error; a failed acquire may require close.
 */
nx_result_t
nx_stm32_spi_frames_prepare(nx_stm32_spi_frames_state_t* state,
                            const nx_stm32_spi_frames_cold_config_t* config,
                            nx_stm32_spi_frames_cold_t* receipt);
/** \brief Join the engine then restore exactly its original cold state.
 * \param[in,out] state: The same owner, serviced until its borrow settles.
 * \param[in] config: The immutable tuple supplied to prepare.
 * \param[in,out] receipt: Actual effects, cleared only after readback.
 * \return Success after restoration; BUSY/IO retains cleanup duty.
 */
nx_result_t
nx_stm32_spi_frames_close(nx_stm32_spi_frames_state_t* state,
                          const nx_stm32_spi_frames_cold_config_t* config,
                          nx_stm32_spi_frames_cold_t* receipt);

/** \brief One actual C owner for the selected IRQ frames personality. */
extern const nx_spi_frames_ops_t nx_stm32_spi_frames_ops;
/** \brief Pure wire/source admission, shared with exact cold construction.
 * \param[in] state: Zero unbound storage without a retained request.
 * \param[in] config: Immutable complete register and wire tuple.
 * \return Success or rejection without any register write or retained borrow.
 */
nx_result_t
nx_stm32_spi_frames_preflight(const nx_stm32_spi_frames_state_t* state,
                              const nx_stm32_spi_frames_config_t* config);
/** \brief Validate and bind a prepared register owner before activation.
 * \param[in,out] state: Zero static state, kept until verified stop.
 * \param[in] config: Complete immutable facts, retained until cold close.
 * \return Success or error; failed acquisition retains config for stop.
 * \note Unmasked Thread-only. Cold resource admission owns GPIO/APB/NVIC.
 */
nx_result_t
nx_stm32_spi_frames_initialize(nx_stm32_spi_frames_state_t* state,
                               const nx_stm32_spi_frames_config_t* config);
/** \brief Move bounded data and latch faults for the exact fixed publisher.
 * \param[in,out] state: Same live owner; no factory lookup or application call.
 * \note Generated IRQ only. Settlement is reserved for the Thread executor.
 */
void nx_stm32_spi_frames_irq(nx_stm32_spi_frames_state_t* state);

#ifdef __cplusplus
}
#endif
#endif

/**
 * \file            stm32h743_spi_wire.h
 * \brief           Bounded H743 SPI wire observations and leased pin control
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_SPI_WIRE_H
#define NX_STM32H743_SPI_WIRE_H
#include "stm32h743_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_STM32H743_SPI_BYTE_CONFIG (7U | (7U << SPI_CFG1_CRCSIZE_Pos))
#define NX_STM32H743_SPI_FIFO_LEVELS                                           \
    ((uint32_t)(SPI_SR_RXPLVL | SPI_SR_RXWNE | SPI_SR_RXP))
/**
 * \brief           Observe the exact V/default-trim SPI4 clock owner.
 * \note            Caller supplies validated stable state. Non-V UNSUPPORTED
 *                  precedes V-only register reads; other failures are STATE.
 *                  Reads only, without acquiring ownership or adopting clocks.
 */
nx_result_t
nx_stm32h743_spi_wire_clock_ready(const nx_stm32h743_spi_state_t* state);
/**
 * \brief           Select a bounded power-of-two prescaler without MMIO.
 * \param[in]       clock_hz: Selected nominal 32/64MHz or compiled high96MHz.
 * \param[in]       maximum_hz: Positive maximum, never a measured frequency.
 * \param[out]      divisor: Divisor 2..256, written only on success.
 * \param[out]      baud: MBR encoding 0..7, written only on success.
 * \return          SUCCESS, INVALID or UNSUPPORTED for unreachable rates.
 * \note            At most seven prescaler steps; no allocation or time read.
 */
nx_result_t nx_stm32h743_spi_wire_divider(uint32_t clock_hz,
                                          uint32_t maximum_hz,
                                          uint32_t* divisor, uint32_t* baud);
/**
 * \brief           Compute one nominal same-source SCK period plus two ticks.
 * \return          Quiet interval in System microsecond units, or zero for
 *                  invalid clock/divisor. No MMIO, deadline or epoch storage.
 * \note            The owner must prove the selected TIM/SPI clock ratio and
 *                  start a fresh deadline after actual stop. This is neither
 *                  a wait nor a physical clock measurement.
 */
uint32_t nx_stm32h743_spi_wire_quiet_us(uint32_t clock_hz, uint32_t divisor);
/** \brief Observe the already exclusively leased PE12/13/14 route and gate. */
bool nx_stm32h743_spi_wire_pins_ready(void);
/** \brief Observe push-pull output mode for a validated owned GPIO mask. */
bool nx_stm32h743_spi_wire_cs_driven(const nx_stm32h743_gpio_state_t* gpio,
                                     uint32_t mask);
/**
 * \brief           Return already leased SCK to AF5 with actual readback.
 * \note            Owner has kept CS high and configured SPI CPOL/AFCNTR.
 *                  No new lease is granted by matching register values.
 */
nx_result_t nx_stm32h743_spi_wire_sck_alternate(void);
/**
 * \brief           Hold CPOL through GPIO before the owner's exclusive reset.
 * \note            Caller has proved wire stop/quiet and CS high or the
 *                  existing never-selected cold path. No waiting or reset.
 */
nx_result_t nx_stm32h743_spi_wire_sck_idle(nx_stm32h743_spi_state_t* state);
/**
 * \brief           Issue one suspend command without replaying CSTART.
 * \note            Validated owner observed CSTART. Zero written to read/set
 *                  CSTART does not clear it. No poll, acknowledgement or wait.
 */
void nx_stm32h743_spi_wire_suspend_once(nx_stm32h743_spi_state_t* state);
/**
 * \brief           Observe a stopped SUSP or finite EOT/TXC event once.
 * \return          True only for CSTART0 with SUSP, or nonzero TSIZE with both
 *                  EOT/TXC. Does not prove the independent trailing-edge quiet.
 * \note            Reads CR1/SR/CR2 only. TXC copies EOT for nonzero TSIZE.
 *                  No flag clearing, snapshot retention or request ownership.
 */
bool nx_stm32h743_spi_wire_stopped(const nx_stm32h743_spi_state_t* state);
/**
 * \brief           Clear SUSP then disable SPI and observe FIFO/enable bits.
 * \note            Owner has proved stop and quiet and drained received data.
 *                  Unsent TXFIFO may be flushed by SPE0. No polling or reset;
 *                  failure retains the caller's cleanup responsibility.
 */
nx_result_t nx_stm32h743_spi_wire_disable(nx_stm32h743_spi_state_t* state);
/**
 * \brief           Release a selected CS only after a real high readback.
 * \note            Owner has proved stop/quiet/disable. Never-selected NULL
 *                  CS makes no GPIO transition; failure keeps the CS pointer.
 */
nx_result_t nx_stm32h743_spi_wire_release_cs(nx_stm32h743_spi_state_t* state);
/**
 * \brief           Check both exclusive SPI reset edges and idle registers.
 * \note            Caller has proved wire/quiet/CS/SCK withdrawal. This leaf
 *                  grants no reset ownership, retries no writes and never
 *                  changes a request or ordinary provider lifecycle fields.
 */
nx_result_t nx_stm32h743_spi_wire_reset(nx_stm32h743_spi_state_t* state);
#ifdef __cplusplus
}
#endif
#endif

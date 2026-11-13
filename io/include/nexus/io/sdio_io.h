/**
 * \file            sdio_io.h
 * \brief           Typed SDIO function control and finite CMD53 borrows
 * \author          Nexus Team
 */
#ifndef NEXUS_SDIO_IO_H
#define NEXUS_SDIO_IO_H
#include "nexus/core/request.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Copied function facts; device-specific CIS tuples belong to drivers.
 */
typedef struct {
    uint32_t cis_address;
    uint16_t block_bytes;
    uint8_t interface_code;
} nx_sdio_function_info_t;

/** \brief Exact discovered card session, never inferred from an SDIO route. */
typedef struct {
    nx_sdio_function_info_t functions[7];
    uint32_t common_cis_address;
    uint8_t function_count;
    uint8_t enabled_functions;
    uint8_t interrupt_functions;
    bool memory_present;
    bool interrupt_latched;
    bool interrupt_armed;
} nx_sdio_io_info_t;

/**
 * \brief Copied selected-host transfer limits, independent of card readiness.
 * \note Positive alignment and unit_bytes_multiple apply to every payload and
 *       unit. max_blocks bounds block mode; byte mode contains one unit of at
 *       most 512 bytes. The function's configured FBR size must also match.
 *       These facts grant neither memory access nor initialization authority.
 */
typedef struct {
    uint32_t max_blocks;
    uint16_t max_block_bytes;
    uint16_t buffer_alignment;
    uint16_t unit_bytes_multiple;
    bool power_of_two_block_bytes;
} nx_sdio_io_limits_t;

/**
 * \brief One finite function-addressed SDIO byte or block request.
 * \note Initialize and prepare base; configure data, data_bytes, blocks and
 *       write. In byte mode blocks is one and unit_bytes is 1..512. In block
 *       mode unit_bytes matches the explicitly configured FBR size and blocks
 *       is positive and provider-bounded. The complete descriptor and payload
 *       remain borrowed until acquire-observed base SETTLED. Buffer alignment,
 *       supported unit sizes and transfer limits belong to the selected host;
 *       submission never rounds, allocates or substitutes a transfer mode.
 */
typedef struct {
    nx_request_t base;
    void* data;
    size_t data_bytes;
    uint32_t blocks;
    uint32_t address;
    uint16_t unit_bytes;
    uint8_t function;
    bool write;
    bool increment;
    bool block_mode;
} nx_sdio_io_request_t;

/** \brief One shared host owner controls standard functions one through seven.
 */
typedef struct {
    nx_result_t (*inspect)(void* context, nx_sdio_io_info_t* info,
                           nx_time_us_t deadline);
    nx_result_t (*enable)(void* context, uint8_t function, bool enabled,
                          nx_time_us_t deadline);
    nx_result_t (*block_size)(void* context, uint8_t function, uint16_t bytes,
                              nx_time_us_t deadline);
    nx_result_t (*read_byte)(void* context, uint8_t function, uint32_t address,
                             uint8_t* value, nx_time_us_t deadline);
    nx_result_t (*write_byte)(void* context, uint8_t function, uint32_t address,
                              uint8_t value, nx_time_us_t deadline);
    nx_result_t (*interrupts)(void* context, uint8_t functions,
                              nx_time_us_t deadline);
    nx_result_t (*pending)(void* context, uint8_t* functions,
                           nx_time_us_t deadline);
    nx_result_t (*submit)(void* context, nx_sdio_io_request_t* request);
    nx_result_t (*cancel)(void* context, nx_sdio_io_request_t* request);
    void (*service)(void* context);
    nx_result_t (*sync)(void* context, nx_time_us_t deadline);
    nx_result_t (*limits)(const void* context, nx_sdio_io_limits_t* limits);
    nx_result_t (*activate)(void* context, nx_time_us_t deadline);
    nx_result_t (*stop)(void* context, nx_time_us_t deadline);
} nx_sdio_io_ops_t;

/** \brief Immutable optional face; uses the same physical block controller ID.
 */
typedef struct {
    const nx_sdio_io_ops_t* ops;
    void* context;
} nx_sdio_io_port_t;

/**
 * \brief Activate the selected SDIO function-card session.
 * \param[in] port: Prepared stopped host under one serialized task owner.
 * \param[in] deadline: Finite absolute host-clock deadline.
 * \return SUCCESS after discovery, or the actual admission/protocol error.
 * \note Uses the same physical owner as all function operations. No block
 *       geometry, implicit fallback or worker is created. A failed activation
 *       can retain protocol effects; stop must establish session withdrawal.
 */
nx_result_t nx_sdio_io_activate(const nx_sdio_io_port_t* port,
                                nx_time_us_t deadline);
/**
 * \brief Stop function activity and return the prepared host to idle.
 * \param[in] port: Same session owner; join requests before stopping.
 * \param[in] deadline: Finite absolute host-clock deadline including shutdown.
 * \return SUCCESS after withdrawal; BUSY retains any outstanding request.
 * \note Cancel and service active requests first. Success ends the card
 *       session, not Platform ownership of pins, IRQs or clocks. A failed stop
 *       retains the same host and protocol recovery responsibility.
 */
nx_result_t nx_sdio_io_stop(const nx_sdio_io_port_t* port,
                            nx_time_us_t deadline);
/**
 * \brief Query host limits without discovering or activating a card.
 * \param[in] port: Prepared host under one serialized task owner.
 * \param[out] limits: Aligned distinct writable output, unchanged on rejection.
 * \return SUCCESS, INVALID, STATE, CONTEXT, BUSY or UNSUPPORTED.
 * \note No MMIO, time access, allocation or borrow. Query before transfers;
 *       an outstanding request makes this output operation BUSY. Success
 *       describes the selected implementation, not a live card or hardware
 *       qualification. The same host validates every submitted request.
 */
nx_result_t nx_sdio_io_limits(const nx_sdio_io_port_t* port,
                              nx_sdio_io_limits_t* limits);
/**
 * \brief Read bounded CCCR/FBR facts and CIS pointers into a copied snapshot.
 * \param[in] port: Explicitly activated SDIO I/O session, same host owner.
 * \param[out] info: Distinct caller storage, retained only during this call.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success or actual control/protocol error, never invented functions.
 * \note All control calls require one privileged unmasked Thread executor.
 *       Domain activation/stop owns this same physical host. No
 *       function-specific register policy, CIS tuple interpretation or worker.
 */
nx_result_t nx_sdio_io_inspect(const nx_sdio_io_port_t* port,
                               nx_sdio_io_info_t* info, nx_time_us_t deadline);
/**
 * \brief Change IOEx and wait for the exact function's IORx transition.
 * \param[in] port: Same serialized idle session, no borrowed request.
 * \param[in] function: Discovered function number one through seven.
 * \param[in] enabled: Explicit requested function state.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Actual ready outcome; a timeout can leave the function enabled.
 */
nx_result_t nx_sdio_io_enable(const nx_sdio_io_port_t* port, uint8_t function,
                              bool enabled, nx_time_us_t deadline);
/**
 * \brief Program and verify the function FBR's explicit power-of-two size.
 * \param[in] port: Same serialized idle session.
 * \param[in] function: Discovered nonzero function number.
 * \param[in] bytes: Exact block bytes, 1..2048, with no rounding.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success or error; a partial two-byte write is not atomic.
 */
nx_result_t nx_sdio_io_block_size(const nx_sdio_io_port_t* port,
                                  uint8_t function, uint16_t bytes,
                                  nx_time_us_t deadline);
/**
 * \brief Read one CMD52 register byte, including function-zero CCCR/FBR/CIS.
 * \param[in] port: Same serialized idle session.
 * \param[in] function: Zero through the discovered function count.
 * \param[in] address: Exact 17-bit function-local address.
 * \param[out] value: Stable distinct byte, borrowed only during this call.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Strict R5/CRC/timeout outcome; failure may retain host uncertainty.
 */
nx_result_t nx_sdio_io_read_byte(const nx_sdio_io_port_t* port,
                                 uint8_t function, uint32_t address,
                                 uint8_t* value, nx_time_us_t deadline);
/**
 * \brief Write one enabled function's register without implicit readback.
 * \param[in] port: Same serialized idle session.
 * \param[in] function: Enabled nonzero function; function-zero writes refused.
 * \param[in] address: Exact 17-bit function-local register address.
 * \param[in] value: Explicit device-register value, not a host command.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Actual R5 status; write side effects are not rolled back on timeout.
 * \note CCCR/FBR configuration stays with the typed host operations. Device
 *       interrupt acknowledgement belongs to the relevant function driver.
 */
nx_result_t nx_sdio_io_write_byte(const nx_sdio_io_port_t* port,
                                  uint8_t function, uint32_t address,
                                  uint8_t value, nx_time_us_t deadline);
/**
 * \brief Set IENx and explicitly arm the selected host interrupt hint.
 * \param[in] port: Same serialized idle host owner.
 * \param[in] functions: Function bits one through seven; zero disables all.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Actual programming outcome, with master enable derived from mask.
 * \note IRQ latches a hint and masks its level source. Read pending, service
 *       device registers, then explicitly rearm; no OS callback or worker.
 *       Successful data completion preserves an unconsumed arm. Error,
 *       cancellation and sync require an explicit new arm.
 */
nx_result_t nx_sdio_io_interrupts(const nx_sdio_io_port_t* port,
                                  uint8_t functions, nx_time_us_t deadline);
/**
 * \brief Read authoritative CCCR pending function bits without acknowledging.
 * \param[in] port: Same serialized idle host owner.
 * \param[out] functions: Copied pending bits, unchanged on rejection/error.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Actual CMD52 outcome; host IRQ status is only a hint.
 */
nx_result_t nx_sdio_io_pending(const nx_sdio_io_port_t* port,
                               uint8_t* functions, nx_time_us_t deadline);
/**
 * \brief Admit a finite byte/block CMD53 sequence to the same host executor.
 * \param[in] port: Activated single-owner session, no other live transfer.
 * \param[in,out] request: Complete READY enclosing descriptor and payload.
 * \return SUCCESS means ACCEPTED; rejection retains no reference or effect.
 * \note Service advances polling, IRQ or DMA selection without a worker.
 *       The selected host validates buffer and unit constraints before
 *       acceptance; no bounce allocation or implicit mode fallback occurs.
 *       Cancellation and timeout retain all borrows until actual drain and
 *       SETTLED. Whole confirmed units form the transferred byte prefix.
 */
nx_result_t nx_sdio_io_submit(const nx_sdio_io_port_t* port,
                              nx_sdio_io_request_t* request);
/**
 * \brief Begin cancellation without reclaiming the enclosing request.
 * \param[in] port: Same serialized execution owner.
 * \param[in,out] request: Exact live stable request, not a stale pointer.
 * \return SUCCESS begins drain; STATE for a different request.
 */
nx_result_t nx_sdio_io_cancel(const nx_sdio_io_port_t* port,
                              nx_sdio_io_request_t* request);
/**
 * \brief Advance the selected engine, deadline and complete withdrawal.
 * \param[in] port: Same privileged unmasked Thread executor.
 * \note QUARANTINED borrows remain live; later service may retry cleanup.
 */
void nx_sdio_io_service(const nx_sdio_io_port_t* port);
/**
 * \brief Explicitly abort any uncertain function transfer and prove readiness.
 * \param[in] port: Same host, no request borrow, no implicit replay/reprobe.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success after host drain and actual CCCR function-ready evidence.
 * \note Leaves the host function-interrupt hint disarmed. Explicitly rearm
 *       after device-register recovery; sync never replays device payloads.
 */
nx_result_t nx_sdio_io_sync(const nx_sdio_io_port_t* port,
                            nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif

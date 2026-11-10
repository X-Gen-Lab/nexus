/**
 * \file            sdio_io.h
 * \brief           Typed SDIO function control and finite CMD53 borrows
 * \author          Nexus Team
 */
#ifndef NEXUS_SDIO_IO_H
#define NEXUS_SDIO_IO_H
#include "nexus/io/block_async.h"
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
 * \brief One finite CMD53 request, with the existing shared request storage.
 * \note Initialize and prepare transfer.base; configure transfer.data,
 *       data_bytes, blocks and write. transfer.first_block must remain zero.
 *       In byte mode blocks is one and unit_bytes is 1..512. In block mode
 *       unit_bytes matches the explicitly configured FBR size and blocks is
 *       positive and provider-bounded. The complete enclosing object and
 *       payload remain borrowed until acquire-observed transfer.base SETTLED.
 *       Payloads require four-byte alignment; DMA requires sixteen-byte
 *       alignment and a whole four-word burst in each unit. Never submit
 *       this interior transfer as an ordinary block request.
 */
typedef struct {
    nx_block_request_t transfer;
    uint32_t address;
    uint16_t unit_bytes;
    uint8_t function;
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
} nx_sdio_io_ops_t;

/** \brief Immutable optional face; uses the same physical block controller ID.
 */
typedef struct {
    const nx_sdio_io_ops_t* ops;
    void* context;
} nx_sdio_io_port_t;

/**
 * \brief Read bounded CCCR/FBR facts and CIS pointers into a copied snapshot.
 * \param[in] port: Explicitly activated SDIO I/O session, same host owner.
 * \param[out] info: Distinct caller storage, retained only during this call.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success or actual control/protocol error, never invented functions.
 * \note All control calls require one privileged unmasked Thread executor.
 *       Ordinary block activation/stop owns this same physical host. No
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
 *       DMA requires aligned whole four-word bursts; no bounce allocation.
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

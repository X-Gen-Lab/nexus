/**
 * \file            modbus_rtu.h
 * \brief           Bounded Modbus RTU core and explicit RS485 ports
 * \author          Nexus Team
 */
#ifndef NX_MODBUS_RTU_H
#define NX_MODBUS_RTU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NX_MODBUS_ADU_MAX      256u
#define NX_MODBUS_REGISTER_MAX 125u

typedef enum {
    NX_MODBUS_OK = 0,
    NX_MODBUS_ILLEGAL_ADDRESS = 2,
    NX_MODBUS_ILLEGAL_VALUE = 3,
    NX_MODBUS_DEVICE_FAILURE = 4,
    NX_MODBUS_DEVICE_BUSY = 6
} nx_modbus_register_result_t;

/* Task context only. The caller serializes feed/poll/recover. No allocations.
 * read returns a coherent snapshot; write_atomic either commits EVERY value or
 * changes NONE. authorize is required and applies to broadcasts as well.
 * Critical writes check product authorization/interlocks here and recheck them
 * atomically inside write_atomic if product state has concurrent owners. RTU
 * itself authenticates neither a peer nor a request. All callbacks are bounded.
 */
typedef struct {
    void* context;
    nx_modbus_register_result_t (*read)(void*, uint16_t, uint16_t, uint16_t*);
    nx_modbus_register_result_t (*authorize_write)(void*, uint16_t, uint16_t,
                                                   const uint16_t*);
    nx_modbus_register_result_t (*write_atomic)(void*, uint16_t, uint16_t,
                                                const uint16_t*);
} nx_modbus_registers_t;

/* All deadlines are absolute monotonic microseconds. write may use DMA, but
 * drain must observe the final stop bit before success. After any write/drain
 * error, abort_and_settle MUST stop DMA and settle all callbacks before
 * success. If abort fails the link is latched FAULTED, its TX storage is
 * retained, and DE remains asserted until recover settles the port. This is a
 * port failure, not a normal timeout. A port must never retain bytes after
 * successful drain or abort. direction(false) must leave the transceiver in
 * receive mode.
 */
typedef struct {
    void* context;
    uint64_t (*now_us)(void*);
    bool (*direction)(void*, bool transmit);
    bool (*write)(void*, const uint8_t*, size_t, uint64_t deadline_us);
    bool (*drain)(void*, uint64_t deadline_us);
    bool (*abort_and_settle)(void*, uint64_t deadline_us);
} nx_modbus_rs485_port_t;

typedef enum {
    NX_MODBUS_RX_IDLE = 0,
    NX_MODBUS_RX_FRAME,
    NX_MODBUS_RX_DISCARD,
    NX_MODBUS_LINK_FAULTED
} nx_modbus_link_state_t;

typedef struct {
    uint64_t frames;
    uint64_t accepted;
    uint64_t crc_errors;
    uint64_t timing_errors;
    uint64_t overflows;
    uint64_t exceptions;
    uint64_t broadcasts;
    uint64_t tx_errors;
    uint64_t clock_errors;
} nx_modbus_counters_t;

typedef struct {
    uint8_t address; /* 1..247; zero is broadcast, never a device identity. */
    uint32_t baudrate;
    uint8_t bits_per_character; /* start + data + parity + stop, e.g. 11. */
    uint32_t tx_timeout_us;
    uint32_t abort_timeout_us;
    nx_modbus_registers_t registers;
    nx_modbus_rs485_port_t port;
} nx_modbus_config_t;

/* Public storage permits static allocation. Treat fields as private. */
typedef struct {
    nx_modbus_config_t config;
    nx_modbus_counters_t counters;
    nx_modbus_link_state_t state;
    uint32_t t1_5_us;
    uint32_t t3_5_us;
    uint64_t last_byte_us;
    uint64_t last_event_us;
    uint16_t rx_length;
    uint16_t tx_length;
    uint8_t rx[NX_MODBUS_ADU_MAX];
    uint8_t tx[NX_MODBUS_ADU_MAX];
    uint16_t values[NX_MODBUS_REGISTER_MAX];
    bool initialized;
    bool clock_started;
} nx_modbus_rtu_t;

/**
 * \brief           Compute standard Modbus CRC16 over caller bytes
 * \param[in]       bytes: Input, NULL only when length is zero
 * \param[in]       length: Byte count
 * \return          CRC16; zero for invalid NULL input with nonzero length
 */
uint16_t nx_modbus_crc16(const uint8_t* bytes, size_t length);
/**
 * \brief           Initialize an unused or fully quiesced protocol instance
 * \param[out]      server: Caller-owned storage, no existing transport borrow
 * \param[in]       config: Explicit register/RS485 ports and finite budgets
 * \return          True when initialized into RX silence acquisition
 * \note            Task-only; configuration is copied, contexts stay live.
 *                  First direction(false) failure is reported; no worker or
 *                  UART is created. Reinitializing a faulted TX borrow is
 *                  prohibited; stop/recover it first.
 */
bool nx_modbus_rtu_init(nx_modbus_rtu_t* server,
                        const nx_modbus_config_t* config);
/**
 * \brief           Feed an externally timestamped UART byte
 * \param[in,out]   server: Initialized instance, serialized by caller
 * \param[in]       byte: Wire byte
 * \param[in]       arrival_us: Capture timestamp in port monotonic domain
 * \return          True if byte joined the frame, false for discard/error
 * \note            Task-only. IRQ queues are external, bounded and explicit;
 *                  queue overflow must call rx_error. No live UART discovery or
 *                  ISR callback.
 */
bool nx_modbus_rtu_feed(nx_modbus_rtu_t* server, uint8_t byte,
                        uint64_t arrival_us);
/**
 * \brief           Service RTU silence and bounded synchronous RS485 response
 * \param[in,out]   server: Initialized instance, single execution owner
 * \param[in]       now_us: Snapshot in the same monotonic domain
 * \return          True for successful progress, false for clock/transport
 *                  error
 * \note            Task-only, explicit external scheduling. RS485 drain must
 *                  observe the final stop bit. Failed abort retains TX bytes
 *                  and asserted DE in LINK_FAULTED; keep instance/port alive
 *                  until successful recover or stop.
 */
bool nx_modbus_rtu_poll(nx_modbus_rtu_t* server, uint64_t now_us);
/**
 * \brief           Discard an errored or overrun receive frame
 * \param[in,out]   server: Initialized serialized instance
 * \param[in]       now_us: Error timestamp in port's monotonic domain
 * \note            Task-only. Does not discard a retained faulted TX borrow.
 */
void nx_modbus_rtu_rx_error(nx_modbus_rtu_t* server, uint64_t now_us);
/**
 * \brief           Settle retained TX then restore receive silence acquisition
 * \param[in,out]   server: Initialized serialized instance with live ports
 * \return          True only after actual port abort/drain and receive
 *                  direction
 * \note            Explicit task maintenance action, no fabricated recovery.
 */
bool nx_modbus_rtu_recover(nx_modbus_rtu_t* server);
/**
 * \brief           Settle the port before reclaiming protocol storage
 * \param[in,out]   server: Initialized, exclusively owned instance
 * \return          True when no transport borrow remains and instance is
 *                  stopped; false preserves fault state, TX bytes and all port
 *                  lifetime obligations
 * \note            Stop future feed/poll users first. No future use except
 *                  explicit initialization is allowed after successful stop.
 */
bool nx_modbus_rtu_stop(nx_modbus_rtu_t* server);

#ifdef __cplusplus
}
#endif
#endif

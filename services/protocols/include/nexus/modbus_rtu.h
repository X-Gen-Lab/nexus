#ifndef NEXUS_MODBUS_RTU_H
#define NEXUS_MODBUS_RTU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXUS_MODBUS_ADU_MAX 256u
#define NEXUS_MODBUS_REGISTER_MAX 125u

typedef enum {
    NEXUS_MODBUS_OK = 0,
    NEXUS_MODBUS_ILLEGAL_ADDRESS = 2,
    NEXUS_MODBUS_ILLEGAL_VALUE = 3,
    NEXUS_MODBUS_DEVICE_FAILURE = 4,
    NEXUS_MODBUS_DEVICE_BUSY = 6
} nexus_modbus_register_result_t;

/* Task context only. The caller serializes feed/poll/recover. No allocations.
 * read returns a coherent snapshot; write_atomic either commits EVERY value or
 * changes NONE. authorize is required and applies to broadcasts as well.
 * Critical writes check product authorization/interlocks here and recheck them
 * atomically inside write_atomic if product state has concurrent owners. RTU
 * itself authenticates neither a peer nor a request. All callbacks are bounded.
 */
typedef struct {
    void* context;
    nexus_modbus_register_result_t (*read)(void*, uint16_t, uint16_t, uint16_t*);
    nexus_modbus_register_result_t (*authorize_write)(void*, uint16_t, uint16_t,
                                                     const uint16_t*);
    nexus_modbus_register_result_t (*write_atomic)(void*, uint16_t, uint16_t,
                                                  const uint16_t*);
} nexus_modbus_registers_t;

/* All deadlines are absolute monotonic microseconds. write may use DMA, but
 * drain must observe the final stop bit before success. After any write/drain
 * error, abort_and_settle MUST stop DMA and settle all callbacks before success.
 * If abort fails the link is latched FAULTED, its TX storage is retained, and
 * DE remains asserted until recover settles the port. This is a port failure,
 * not a normal timeout. A port must never retain bytes after successful drain
 * or abort. direction(false) must leave the transceiver in receive mode.
 */
typedef struct {
    void* context;
    uint64_t (*now_us)(void*);
    bool (*direction)(void*, bool transmit);
    bool (*write)(void*, const uint8_t*, size_t, uint64_t deadline_us);
    bool (*drain)(void*, uint64_t deadline_us);
    bool (*abort_and_settle)(void*, uint64_t deadline_us);
} nexus_modbus_rs485_port_t;

typedef enum {
    NEXUS_MODBUS_RX_IDLE = 0,
    NEXUS_MODBUS_RX_FRAME,
    NEXUS_MODBUS_RX_DISCARD,
    NEXUS_MODBUS_LINK_FAULTED
} nexus_modbus_link_state_t;

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
} nexus_modbus_counters_t;

typedef struct {
    uint8_t address; /* 1..247; zero is broadcast, never a device identity. */
    uint32_t baudrate;
    uint8_t bits_per_character; /* start + data + parity + stop, e.g. 11. */
    uint32_t tx_timeout_us;
    uint32_t abort_timeout_us;
    nexus_modbus_registers_t registers;
    nexus_modbus_rs485_port_t port;
} nexus_modbus_config_t;

/* Public storage permits static allocation. Treat fields as private. */
typedef struct {
    nexus_modbus_config_t config;
    nexus_modbus_counters_t counters;
    nexus_modbus_link_state_t state;
    uint32_t t1_5_us;
    uint32_t t3_5_us;
    uint64_t last_byte_us;
    uint64_t last_event_us;
    uint16_t rx_length;
    uint16_t tx_length;
    uint8_t rx[NEXUS_MODBUS_ADU_MAX];
    uint8_t tx[NEXUS_MODBUS_ADU_MAX];
    uint16_t values[NEXUS_MODBUS_REGISTER_MAX];
    bool initialized;
    bool clock_started;
} nexus_modbus_rtu_t;

uint16_t nexus_modbus_crc16(const uint8_t* bytes, size_t length);
bool nexus_modbus_rtu_init(nexus_modbus_rtu_t*, const nexus_modbus_config_t*);
/* Capture byte arrival timestamps at the UART. ISR should enqueue bytes into
 * a bounded SPSC queue; these APIs execute in the communication task. A queue
 * overrun must be reported through rx_error rather than silently dropping bytes.
 */
bool nexus_modbus_rtu_feed(nexus_modbus_rtu_t*, uint8_t byte, uint64_t arrival_us);
bool nexus_modbus_rtu_poll(nexus_modbus_rtu_t*, uint64_t now_us);
void nexus_modbus_rtu_rx_error(nexus_modbus_rtu_t*, uint64_t now_us);
/* Explicit maintenance action. Does not fabricate successful port recovery. */
bool nexus_modbus_rtu_recover(nexus_modbus_rtu_t*);

#ifdef __cplusplus
}
#endif
#endif

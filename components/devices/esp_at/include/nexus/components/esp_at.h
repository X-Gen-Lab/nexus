/**
 * \file            esp_at.h
 * \brief           Selected ESP32-C3 ESP-AT 4.1.1.0 single TCP module profile
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_ESP_AT_H
#define NEXUS_COMPONENTS_ESP_AT_H
#include "nexus/components/at_client.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit ordinary task phase, not a hardware qualification. */
typedef enum {
    NX_ESP_AT_COLD,
    NX_ESP_AT_CONFIGURING,
    NX_ESP_AT_READY,
    NX_ESP_AT_JOINING,
    NX_ESP_AT_CONNECTING,
    NX_ESP_AT_CONNECTED,
    NX_ESP_AT_SENDING,
    NX_ESP_AT_CLOSING,
    NX_ESP_AT_CLOSED,
    NX_ESP_AT_FAILED
} nx_esp_at_phase_t;
/** \brief Exact caller buffers and an already owned copying UART stream. */
typedef struct {
    nx_byte_stream_t uart;
    nx_clock_t clock;
    uint8_t* line;
    size_t line_capacity;
    uint8_t* command;
    size_t command_capacity;
    uint8_t* transmit;
    size_t transmit_capacity;
    uint8_t* receive;
    size_t receive_capacity;
} nx_esp_at_config_t;
/** \brief Copied observations; no method proves remote application delivery.
 */
typedef struct {
    nx_esp_at_phase_t phase;
    nx_result_t failure;
    size_t received_bytes;
    size_t pending_bytes;
    bool firmware_verified;
    bool wifi;
    bool connected;
} nx_esp_at_status_t;
/** \brief Selected-only caller state; all fields private after initialization.
 */
typedef struct {
    nx_at_client_t client;
    nx_esp_at_config_t config;
    nx_byte_stream_t stream;
    nx_time_us_t deadline;
    nx_time_us_t last_time;
    size_t received_bytes;
    size_t receive_head;
    size_t pending_bytes;
    size_t transmitted_bytes;
    uint8_t setup;
    nx_esp_at_phase_t phase;
    nx_result_t failure;
    bool initialized;
    bool active;
    uint8_t firmware_stage;
    bool wifi;
    bool connected;
    bool command_completed;
    bool connect_seen;
    bool close_seen;
    bool prompt_seen;
    bool send_accepted;
} nx_esp_at_t;
/**
 * \brief Bind a selected firmware profile without probing/resetting hardware.
 * \param[out] module: Unused/quiescent caller state, disjoint from all buffers.
 * \param[in] config: Stable UART/clock, line >=128 B, command >=224 B,
 *                    positive RX and 1..8192 B copied TX capacities.
 * \return SUCCESS or INVALID with no UART I/O or allocation.
 * \note Single serialized task owns all methods and the underlying UART.
 *       Buffers, UART/context and clock outlive every upper consumer and close.
 *       Power, reset, UART pins/rate/flow control and initial drained boundary
 *       are product facts. This profile selects active receive, single TCP,
 *       normal transmission and ESP32-C3 version 4.1.1.0; no scan or fallback.
 */
nx_result_t nx_esp_at_init(nx_esp_at_t* module,
                           const nx_esp_at_config_t* config);
/**
 * \brief Admit GMR verification and the fixed volatile station/TCP setup.
 * \param[in,out] module: Initialized COLD same-owner state.
 * \param[in] deadline: Finite total setup deadline in the UART clock domain.
 * \return SUCCESS means command accepted, not READY; INVALID/STATE/BUSY or
 *         TIMEOUT before effects. service explicitly advances the sequence.
 * \note Setup selects SYSSTORE=0, CWMODE=1,0, CIPMUX=0, CIPMODE=0,
 *       CIPDINFO=0 and CIPRECVTYPE=0 only after the exact GMR profile matches.
 */
nx_result_t nx_esp_at_configure(nx_esp_at_t* module, nx_time_us_t deadline);
/**
 * \brief Copy an explicit station join command without retaining credentials.
 * \param[in,out] module: READY metadata, no TCP user or command active.
 * \param[in] ssid: 1..32 bytes, no NUL/CR/LF; quotes/comma/backslash escaped.
 * \param[in] ssid_bytes: Exact SSID bytes.
 * \param[in] password: 8..63 WPA-PSK bytes or NULL for an open network.
 * \param[in] password_bytes: Exact password bytes, zero for an open network.
 * \param[in] deadline: Finite whole join deadline, never renewed by URCs.
 * \return SUCCESS means admission; service waits for OK and WIFI GOT IP.
 * \note No credential is logged or persisted by this wrapper. The selected
 *       module command buffer holds credentials until join completion/recovery;
 *       product controls access to UART/storage and authorizes network choice.
 */
nx_result_t nx_esp_at_join(nx_esp_at_t* module, const char* ssid,
                           size_t ssid_bytes, const char* password,
                           size_t password_bytes, nx_time_us_t deadline);
/**
 * \brief Admit a single IPv4/domain TCP connection using explicit parameters.
 * \param[in,out] module: READY with observed Wi-Fi IP and no old session.
 * \param[in] host: 1..64 ASCII hostname/IPv4 bytes; only letters/digits/.-.
 * \param[in] host_bytes: Exact host bytes, excludes NUL.
 * \param[in] port: Nonzero TCP port.
 * \param[in] deadline: Finite whole connection deadline.
 * \return SUCCESS means admission; CONNECT and OK are both required for state.
 * \note Hostname resolution belongs to the module. Nexus TLS may consume the
 *       returned byte stream with independent trust/time/entropy policy.
 */
nx_result_t nx_esp_at_connect(nx_esp_at_t* module, const char* host,
                              size_t host_bytes, uint16_t port,
                              nx_time_us_t deadline);
/**
 * \brief Progress at most io_budget copying UART read/write calls.
 * \param[in,out] module: Live same-owner metadata and buffers.
 * \param[in] deadline: Finite service budget, capped by an active operation.
 * \param[in] io_budget: Positive maximum individual transport calls.
 * \return SUCCESS for progress, BUSY for no progress, or latched failure.
 * \note No worker, wait, heap or command replay. Header parsing handles binary
 *       immediately after ':' in the same read. RX overflow, malformed length,
 *       unexpected reset/URC, timeout or loss closes admission until recovery.
 *       SEND OK means module stack admission, not peer delivery.
 */
nx_result_t nx_esp_at_service(nx_esp_at_t* module, nx_time_us_t deadline,
                              uint32_t io_budget);
/**
 * \brief Query the existing readonly copied socket face without I/O.
 * \param[in] module: Initialized state, retained through all consumers.
 * \return Stable face or NULL for invalid/uninitialized metadata.
 * \note Task-only immediate calls retain no caller buffers. Writes copy at
 *       most the selected TX capacity and wait for explicit service. Remote
 *       CLOSED drains copied RX before EOF; failed sessions report error.
 */
const nx_byte_stream_t* nx_esp_at_stream(const nx_esp_at_t* module);
/**
 * \brief Admit explicit close after every upper consumer and TX user joined.
 * \param[in,out] module: CONNECTED with no pending copied TX, or CLOSED.
 * \param[in] deadline: Finite close deadline; old operations are not renewed.
 * \return SUCCESS admits close/already closed; BUSY/STATE or actual failure.
 * \note service waits for real CLOSED and OK. CLOSED is terminal for this
 *       session: product drains/resets to a known boundary before recovery and
 *       another connection; no late URC can silently bind a new socket.
 */
nx_result_t nx_esp_at_close(nx_esp_at_t* module, nx_time_us_t deadline);
/**
 * \brief Reset software after actual external module/UART synchronization.
 * \param[in,out] module: Same original metadata, all calls/consumers joined.
 * \return SUCCESS, BUSY, INVALID or STATE for an invalid clock history.
 * \note Product first drains/joins transport and resets the module to its known
 *       boundary. This performs no reset, power action or hardware join.
 * Pending TX is abandoned only after that proof and is never replayed. Next use
 *       explicitly runs configure; initialized buffers remain caller-owned.
 */
nx_result_t nx_esp_at_recover(nx_esp_at_t* module);
/**
 * \brief Copy selected facts and current local state in the owning task.
 * \param[in] module: Initialized stable serialized metadata.
 * \param[out] status: Disjoint output; unchanged on rejection.
 * \return SUCCESS, INVALID or BUSY inside a live service call.
 * \note Observation does not join callbacks, certify firmware or grant release.
 */
nx_result_t nx_esp_at_status(const nx_esp_at_t* module,
                             nx_esp_at_status_t* status);
#ifdef __cplusplus
}
#endif
#endif

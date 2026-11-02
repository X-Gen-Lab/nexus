/**
 * \file            tls.h
 * \brief           Explicit bounded TLS 1.2 client and crypto-domain lifecycle
 * \author          Nexus Team
 */
#ifndef NEXUS_TLS_TLS_H
#define NEXUS_TLS_TLS_H
#include "nexus/io/entropy.h"
#include "nexus/network/byte_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_TLS_ARENA_BYTES    65536U
#define NX_TLS_TX_STAGE_BYTES 512U
#define NX_TLS_DER_MAX        4096U
typedef struct nx_tls_runtime nx_tls_runtime_t;
typedef struct nx_tls_session nx_tls_session_t;
/** \brief Source-quality declaration, never inferred from a health check. */
typedef enum {
    NX_TLS_ENTROPY_UNKNOWN = 0,
    NX_TLS_ENTROPY_FULL_SOURCE
} nx_tls_entropy_quality_t;
/**
 * \brief Explicit trusted UTC, separate from monotonic operation deadlines.
 * \note SUCCESS means trusted Unix seconds. Unknown/untrusted/unavailable time
 *       returns an error. Calendar years are 1970..9999 within the selected
 *       time_t representation. The product owns authentication and clock
 * policy.
 */
typedef struct {
    nx_result_t (*read)(void* context, int64_t* seconds);
    void* context;
} nx_tls_utc_t;
/**
 * \brief One serialized crypto owner, caller arena and finite entropy source.
 * \note Arena is max_align_t-aligned and exactly NX_TLS_ARENA_BYTES. All faces
 *       and opaque contexts outlive every session. FULL_SOURCE explicitly
 *       declares 8 bits/byte to the selected PSA entropy-driver API; software
 *       health checks and this enum do not establish physical qualification.
 *       The profile disables operating-system entropy and seed fallbacks.
 *       Clock advances in one unchanged monotonic domain throughout callbacks.
 */
typedef struct {
    void* arena;
    size_t arena_bytes;
    const nx_entropy_port_t* entropy;
    nx_clock_t clock;
    nx_tls_utc_t utc;
    nx_time_us_t entropy_timeout_us;
    nx_tls_entropy_quality_t entropy_quality;
} nx_tls_runtime_config_t;
/**
 * \brief Client credentials copied by the backend during initialization.
 * \note DER only, no implicit files, key discovery or default trust. hostname
 *       has hostname_length non-NUL bytes followed by NUL, length 1..253.
 *       Mutual-authentication certificate and key are both supplied or absent.
 *       TX staging is exactly 512 caller-owned bytes, disjoint from runtime,
 *       session, arena and transport state. It stays borrowed until abort.
 *       The single serialized executor owns all session and PSA calls.
 */
typedef struct {
    nx_tls_runtime_t* runtime;
    const nx_byte_stream_t* transport;
    const uint8_t* trust_der;
    size_t trust_bytes;
    const uint8_t* client_der;
    size_t client_bytes;
    const uint8_t* key_der;
    size_t key_bytes;
    const char* hostname;
    size_t hostname_length;
    uint8_t* tx_stage;
    size_t tx_stage_bytes;
} nx_tls_config_t;
typedef enum {
    NX_TLS_IDLE = 0,
    NX_TLS_INITIALIZED,
    NX_TLS_HANDSHAKING,
    NX_TLS_ESTABLISHED,
    NX_TLS_CLOSING,
    NX_TLS_CLOSED,
    NX_TLS_FAILED
} nx_tls_state_t;
/** \brief Read-only diagnosis; no snapshot grants permission to operate. */
typedef struct {
    nx_tls_state_t state;
    nx_result_t result;
    int library_error;
    uint32_t verify_flags;
    size_t pending_plaintext;
    bool want_read;
    bool want_write;
    bool peer_closed;
} nx_tls_diagnostic_t;
/**
 * \brief Initialize one explicit PSA/arena domain, never once per connection.
 * \param[in,out] runtime: Zeroed static backend storage.
 * \param[in] config: Configuration copied; referenced resources stay borrowed.
 * \param[in] deadline: Finite absolute initialization deadline.
 * \return Success, permission for undeclared quality, or actual bounded error.
 * \note No worker/heap/lock. A second live domain is BUSY. Every library
 * entropy callback is guarded by the shorter operation/entropy deadline.
 */
nx_result_t nx_tls_runtime_start(nx_tls_runtime_t* runtime,
                                 const nx_tls_runtime_config_t* config,
                                 nx_time_us_t deadline);
/**
 * \brief Stop PSA only after all sessions and outside consumers have joined.
 * \param[in,out] runtime: Exclusive live domain owner.
 * \return BUSY with a session; success frees PSA then wipes the whole arena.
 * \note No session cleanup calls this operation implicitly. Product code must
 *       not use PSA outside this explicit domain or reset its global allocator.
 */
nx_result_t nx_tls_runtime_stop(nx_tls_runtime_t* runtime);
/**
 * \brief Prepare one client using public Mbed TLS APIs and required peer auth.
 * \param[in,out] session: Zeroed static backend storage.
 * \param[in] config: Explicit trust, hostname, optional client identity/port.
 * \param[in] deadline: Finite initialization deadline, including key import.
 * \return Success, BUSY for the occupied domain, or no-admission failure.
 * \note No raw credential input is retained after return. Port and staging
 *       stay borrowed until abort; failures clean partial backend state.
 */
nx_result_t nx_tls_initialize(nx_tls_session_t* session,
                              const nx_tls_config_t* config,
                              nx_time_us_t deadline);
/**
 * \brief Perform one public handshake step under the first locked deadline.
 * \param[in,out] session: Initialized exclusive client.
 * \param[in] deadline: Finite deadline; later calls cannot extend the first.
 * \return Success only after verified auth; BUSY for progress/WANT; fatal
 * error. \note First step publicly reseeds PSA with a finite guard. Trusted UTC
 * is sampled coherently; loss during a library date check is latched and
 *       overrides a successful library result. Renegotiation is disabled.
 */
nx_result_t nx_tls_handshake_step(nx_tls_session_t* session,
                                  nx_time_us_t deadline);
/**
 * \brief Progress the one accepted plaintext staging block without waiting.
 * \param[in,out] session: Live serialized owner.
 * \return Success when no pending bytes; BUSY for WANT; sticky fatal error.
 * \note WANT retries keep the backend's exact original pointer/length. An
 *       expired queued write fails the session; only abort releases its stage.
 */
nx_result_t nx_tls_service(nx_tls_session_t* session);
/**
 * \brief Copy an immediate partial write into the explicit bounded stage.
 * \param[in,out] session: Established serialized client.
 * \param[in] data: Immutable input, not retained after return.
 * \param[in] bytes: Requested bytes; zero performs no I/O.
 * \param[out] accepted: Disjoint copied count; zero on BUSY/error.
 * \param[in] deadline: Finite absolute deadline retained for accepted bytes.
 * \return Success means copied admission, not peer delivery; BUSY or error.
 * \note Original input may be reused immediately. Pending stage is never
 *       overwritten, replayed or discarded on timeout. Late failures are
 * sticky. An invalid aliased count is rejected before writing that count.
 */
nx_result_t nx_tls_try_write(nx_tls_session_t* session, const uint8_t* data,
                             size_t bytes, size_t* accepted,
                             nx_time_us_t deadline);
/**
 * \brief Try to copy authenticated plaintext without retaining caller output.
 * \param[in,out] session: Established serialized client.
 * \param[out] data: Unretained output disjoint from all live session storage.
 * \param[in] bytes: Writable capacity; zero performs no I/O.
 * \param[out] received: Disjoint count; zero on BUSY/error or auth EOF.
 * \param[in] deadline: Finite absolute deadline for this nonblocking attempt.
 * \return Success/bytes, BUSY, authenticated close-notify EOF, or error.
 * \note Raw transport EOF without close-notify is a sticky truncation error.
 *       An expired preflight retains session state and creates no new borrow.
 *       An invalid aliased count is rejected before writing that count.
 *       A pending stage with a later effective deadline returns BUSY without
 *       automatic I/O; the owner explicitly services that earlier acceptance.
 */
nx_result_t nx_tls_try_read(nx_tls_session_t* session, uint8_t* data,
                            size_t bytes, size_t* received,
                            nx_time_us_t deadline);
/**
 * \brief Send close-notify with an explicit first locked deadline.
 * \param[in,out] session: Exclusive live client after producers have joined.
 * \param[in] deadline: Finite absolute deadline; no implicit extension.
 * \return Success means local alert accepted, BUSY for WANT, or sticky error.
 * \note Pending app bytes must first drain. This never closes the underlying
 *       transport or proves the peer consumed data. Abort still frees storage.
 */
nx_result_t nx_tls_close_step(nx_tls_session_t* session, nx_time_us_t deadline);
/**
 * \brief Free synchronous session state and wipe its stage after final access.
 * \param[in,out] session: Exclusive owner, all external facade users joined.
 * \return Success after cleanup (also cold), BUSY if crypto/BIO is executing.
 * \note BUSY retains all storage; the outer owner retries after callback join.
 *       No network I/O or replay. Underlying transport closure remains
 * external. The pinned builtin synchronous crypto profile retains no callback
 * after return. PSA domain/arena stays live for the next explicit session.
 */
nx_result_t nx_tls_abort(nx_tls_session_t* session);
/** \brief Return an existing no-borrow byte face; no start or allocation. */
const nx_byte_stream_t* nx_tls_stream(const nx_tls_session_t* session);
/** \brief Copy current owner diagnosis to unretained disjoint output. */
nx_result_t nx_tls_diagnostic(const nx_tls_session_t* session,
                              nx_tls_diagnostic_t* diagnostic);
#ifdef __cplusplus
}
#endif
#endif

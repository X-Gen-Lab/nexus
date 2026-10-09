/**
 * \file            log.h
 * \brief           Instance-local synchronous bounded log sink
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LOG_H
#define NEXUS_COMPONENTS_LOG_H
#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Structured severity, with filtering before touching a sink. */
typedef enum {
    NX_LOG_DEBUG = 0,
    NX_LOG_INFO,
    NX_LOG_WARNING,
    NX_LOG_ERROR
} nx_log_level_t;

/**
 * \brief           Synchronous caller-injected sink
 * \note            write/flush release every byte buffer before returning even
 *                  on timeout/error; asynchronous/DMA sinks require their own
 *                  settled adapter. Calls are bounded by absolute monotonic
 *                  deadlines, in task context. No sink discovery, allocation,
 *                  formatting, worker or global error handler exists.
 */
typedef struct {
    void* context;
    nx_result_t (*write)(void*, nx_log_level_t, const void*, size_t,
                         nx_time_us_t deadline);
    nx_result_t (*flush)(void*, nx_time_us_t deadline);
} nx_log_sink_port_t;

/**
 * \brief           Logger storage belongs to its caller; shared immutable port
 *                  is copied.
 */
typedef struct {
    nx_log_sink_port_t sink;
    nx_log_level_t minimum;
    uint32_t busy;
    uint32_t stopped;
} nx_log_t;

/**
 * \brief           Initialize an unused logger before exposing it to tasks
 * \param[out]      logger: Unused caller storage
 * \param[in]       sink: Required bounded synchronous sink
 * \param[in]       minimum: Minimum accepted severity
 * \return          SUCCESS or INVALID
 */
nx_result_t nx_log_init(nx_log_t* logger, nx_log_sink_port_t sink,
                        nx_log_level_t minimum);
/**
 * \brief           Write caller bytes, with no implicit allocation/formatting
 * \param[in,out]   logger: Live instance
 * \param[in]       level: Message severity
 * \param[in]       bytes: Message bytes, borrowed only during this call
 * \param[in]       length: Byte count; zero permits NULL
 * \param[in]       deadline: Absolute deadline understood by sink
 * \return          SUCCESS for filtered/sent bytes, BUSY for
 *                  concurrent/recursive writes, STATE after stop, otherwise
 *                  sink error
 * \note            Task multi-producer entry with nonblocking serialization.
 *                  IRQ use is prohibited. Drop/retry policy remains with the
 *                  external caller.
 */
nx_result_t nx_log_write(nx_log_t* logger, nx_log_level_t level,
                         const void* bytes, size_t length,
                         nx_time_us_t deadline);
/**
 * \brief           Flush an explicit sink without creating a background worker
 * \param[in,out]   logger: Live instance
 * \param[in]       deadline: Absolute sink deadline
 * \return          SUCCESS, BUSY, STATE, UNSUPPORTED or sink error
 */
nx_result_t nx_log_flush(nx_log_t* logger, nx_time_us_t deadline);
/**
 * \brief           Reject new writes and test whether the sink has exited
 * \param[in,out]   logger: Live instance; external users must eventually join
 * \return          SUCCESS when no sink call is active, BUSY while draining
 * \note            Task context. A stop caller may retry; no active sink call
 *                  is forcibly cancelled. Join publishers before reclaiming
 *                  logger/sink storage.
 */
nx_result_t nx_log_stop(nx_log_t* logger);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_LOG_H */

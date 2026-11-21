/**
 * \file            xmodem.h
 * \brief           Caller-driven XMODEM CRC-128 receive state machine
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_XMODEM_H
#define NEXUS_COMPONENTS_XMODEM_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Bounded block acceptance; SUCCESS means copied/committed locally. */
typedef nx_result_t (*nx_xmodem_write_t)(void* context, uint32_t block_index,
                                         const uint8_t data[128]);
/** \brief Caller-owned CRC-128 receiver, no UART, files or firmware policy. */
typedef struct {
    nx_xmodem_write_t write;
    void* context;
    nx_time_us_t deadline;
    nx_time_us_t packet_deadline;
    nx_time_us_t last_time;
    uint32_t packet_timeout_us;
    uint32_t block_index;
    uint16_t position;
    uint8_t retries;
    uint8_t retry_limit;
    uint8_t expected;
    uint8_t pending_control;
    uint8_t packet[132];
    nx_result_t result;
    bool active;
    bool complete;
    bool previous_can;
} nx_xmodem_t;
/**
 * \brief           Start CRC-128 receive and queue initial 'C' handshake.
 * \param[out]      receiver: Unused or quiescent caller storage.
 * \param[in]       write: Bounded task callback, no reentry, retains no data.
 * \param[in]       context: Stable callback state until receive completion.
 * \param[in]       now: Finite monotonic start snapshot.
 * \param[in]       deadline: Finite whole-transfer deadline after now.
 * \param[in]       packet_timeout_us: Nonzero bounded packet interval.
 * \param[in]       retry_limit: Nonzero maximum consecutive corrupt/timeouts.
 * \return          SUCCESS or INVALID; no transport I/O or allocation.
 * \note            Packet blocks are always 128 bytes, including sender
 * padding. Product owns exact length, authorization, staging and erase.
 */
nx_result_t nx_xmodem_start(nx_xmodem_t* receiver, nx_xmodem_write_t write,
                            void* context, nx_time_us_t now,
                            nx_time_us_t deadline, uint32_t packet_timeout_us,
                            uint8_t retry_limit);
/**
 * \brief           Consume one received byte under finite session time.
 * \param[in,out]   receiver: Serialized active state.
 * \param[in]       byte: Next wire byte.
 * \param[in]       now: Same-domain nondecreasing finite snapshot.
 * \return          SUCCESS for progress/completion, BUSY when an outgoing
 *                  control byte is pending (input not consumed), or failure.
 * \note            Valid new blocks call write exactly once; duplicate previous
 *                  blocks only ACK. CRC is corruption detection, not signature.
 */
nx_result_t nx_xmodem_feed(nx_xmodem_t* receiver, uint8_t byte,
                           nx_time_us_t now);
/**
 * \brief           Advance timeout/retry without receiving data.
 * \param[in,out]   receiver: Serialized initialized state.
 * \param[in]       now: Same-domain finite monotonic snapshot.
 * \return          SUCCESS, BUSY for active transfer, or latched failure.
 * \note            Caller explicitly schedules progress and transmits queued
 *                  controls; no hidden timer, retransmission task or worker.
 */
nx_result_t nx_xmodem_poll(nx_xmodem_t* receiver, nx_time_us_t now);
/**
 * \brief           Take one queued control byte for copying into transport.
 * \param[in,out]   receiver: Serialized initialized state.
 * \param[out]      control: Distinct output byte, unchanged if no control.
 * \return          SUCCESS, EMPTY or INVALID; caller retains copied byte until
 *                  transport accepts it. This call has no transport effects.
 */
nx_result_t nx_xmodem_control(nx_xmodem_t* receiver, uint8_t* control);
/**
 * \brief           Cancel local transfer after joining all calls/callbacks.
 * \param[in,out]   receiver: Initialized serialized state.
 * \return          CANCELLED, or INVALID; does not undo accepted blocks.
 * \note            Product may send CAN bytes explicitly, then settle
 * transport.
 */
nx_result_t nx_xmodem_cancel(nx_xmodem_t* receiver);
#ifdef __cplusplus
}
#endif
#endif

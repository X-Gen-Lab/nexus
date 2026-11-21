/**
 * \file            xmodem.c
 * \brief           Bounded CRC-128 transfer without image or storage policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/xmodem.h"
#include "nexus/algorithms/crc.h"
#include <string.h>

/** \brief Latch terminal status without undoing accepted storage effects. */
static nx_result_t stop(nx_xmodem_t* r, nx_result_t result) {
    r->active = false;
    r->result = result;
    return result;
}
/** \brief Start a new finite packet interval bounded by the session deadline.
 */
static void interval(nx_xmodem_t* r, nx_time_us_t now) {
    r->packet_deadline =
        now >= r->deadline || r->deadline - now <= r->packet_timeout_us
            ? r->deadline
            : now + r->packet_timeout_us;
}
/** \brief Reject rollback and whole-session expiry before any callback. */
static nx_result_t admit(nx_xmodem_t* r, nx_time_us_t now) {
    if (r == NULL || r->write == NULL || now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!r->active) {
        return r->result;
    }
    if (now < r->last_time) {
        return stop(r, NX_ERROR_STATE);
    }
    r->last_time = now;
    if (now >= r->deadline) {
        return stop(r, NX_ERROR_TIMEOUT);
    }
    return NX_SUCCESS;
}
/** \brief Bound repeated malformed/incomplete packets and schedule recovery. */
static nx_result_t retry(nx_xmodem_t* r, nx_time_us_t now,
                         nx_result_t exhausted) {
    bool handshake = r->position == 0 && r->block_index == 0;
    r->position = 0;
    if (++r->retries >= r->retry_limit) {
        return stop(r, exhausted);
    }
    r->pending_control = handshake ? 'C' : 0x15;
    interval(r, now);
    return NX_SUCCESS;
}
/** \brief Copy the explicit callback and finite receive budgets. */
nx_result_t nx_xmodem_start(nx_xmodem_t* r, nx_xmodem_write_t write,
                            void* context, nx_time_us_t now,
                            nx_time_us_t deadline, uint32_t packet_timeout_us,
                            uint8_t retry_limit) {
    if (r == NULL || write == NULL || now == NX_DEADLINE_NEVER ||
        deadline == NX_DEADLINE_NEVER || deadline <= now ||
        packet_timeout_us == 0 || retry_limit == 0) {
        return NX_ERROR_INVALID;
    }
    memset(r, 0, sizeof(*r));
    r->write = write;
    r->context = context;
    r->deadline = deadline;
    r->packet_timeout_us = packet_timeout_us;
    r->last_time = now;
    r->retry_limit = retry_limit;
    r->expected = 1;
    r->pending_control = 'C';
    r->active = true;
    interval(r, now);
    return NX_SUCCESS;
}
/** \brief Validate sequence/complement/CRC before the single storage callback.
 */
static nx_result_t packet(nx_xmodem_t* r, nx_time_us_t now) {
    const nx_crc_model_t model = {0x1021, 0, 0, 16, false, false};
    uint32_t crc = 0;
    uint32_t final = 0;
    (void)nx_crc_update(&model, r->packet + 2, 128, &crc);
    (void)nx_crc_finish(&model, crc, &final);
    const uint16_t expected =
        (uint16_t)(((uint16_t)r->packet[130] << 8) | r->packet[131]);
    if ((unsigned)r->packet[0] + r->packet[1] != 255U || final != expected) {
        return retry(r, now, NX_ERROR_IO);
    }
    if (r->packet[0] == r->expected) {
        if (r->block_index == UINT32_MAX) {
            return stop(r, NX_ERROR_OVERFLOW);
        }
        /* The callback returns only after it has copied/committed the block. */
        nx_result_t result =
            r->write(r->context, r->block_index, r->packet + 2);
        if (result != NX_SUCCESS) {
            return stop(r, result == NX_ERROR_BUSY ? NX_ERROR_STATE : result);
        }
        ++r->block_index;
        ++r->expected;
    } else if (r->block_index == 0 ||
               r->packet[0] != (uint8_t)(r->expected - 1U)) {
        return stop(r, NX_ERROR_STATE);
    }
    r->position = 0;
    r->retries = 0;
    r->pending_control = 6;
    interval(r, now);
    return NX_SUCCESS;
}
/** \brief Parse bytes without accepting input over an unconsumed control. */
nx_result_t nx_xmodem_feed(nx_xmodem_t* r, uint8_t byte, nx_time_us_t now) {
    nx_result_t result = admit(r, now);
    if (result != NX_SUCCESS || !r->active) {
        return result;
    }
    if (r->pending_control != 0) {
        return NX_ERROR_BUSY;
    }
    if (now >= r->packet_deadline) {
        (void)retry(r, now, NX_ERROR_TIMEOUT);
        return r->active ? NX_ERROR_BUSY : r->result;
    }
    if (r->position != 0) {
        r->packet[r->position - 1] = byte;
        if (++r->position == 133) {
            return packet(r, now);
        }
        return NX_SUCCESS;
    }
    if (byte == 0x18) {
        if (r->previous_can) {
            return stop(r, NX_ERROR_CANCELLED);
        }
        r->previous_can = true;
        return NX_SUCCESS;
    }
    r->previous_can = false;
    if (byte == 1) {
        r->position = 1;
        return NX_SUCCESS;
    }
    if (byte == 4) {
        r->pending_control = 6;
        r->complete = true;
        return stop(r, NX_SUCCESS);
    }
    return retry(r, now, NX_ERROR_IO);
}
/** \brief Advance finite timeout handling without consuming wire bytes. */
nx_result_t nx_xmodem_poll(nx_xmodem_t* r, nx_time_us_t now) {
    nx_result_t result = admit(r, now);
    if (result != NX_SUCCESS || !r->active) {
        return result;
    }
    if (now >= r->packet_deadline) {
        result = retry(r, now, NX_ERROR_TIMEOUT);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return NX_ERROR_BUSY;
}
/** \brief Transfer ownership of one control byte to the external executor. */
nx_result_t nx_xmodem_control(nx_xmodem_t* r, uint8_t* control) {
    if (r == NULL || control == NULL || r->write == NULL ||
        ((uintptr_t)control >= (uintptr_t)r &&
         (uintptr_t)control - (uintptr_t)r < sizeof(*r))) {
        return NX_ERROR_INVALID;
    }
    if (r->pending_control == 0) {
        return NX_ERROR_EMPTY;
    }
    *control = r->pending_control;
    r->pending_control = 0;
    return NX_SUCCESS;
}
/** \brief Cancel locally; product retains accepted data and transport duties.
 */
nx_result_t nx_xmodem_cancel(nx_xmodem_t* r) {
    if (r == NULL || r->write == NULL) {
        return NX_ERROR_INVALID;
    }
    r->pending_control = 0;
    return stop(r, NX_ERROR_CANCELLED);
}

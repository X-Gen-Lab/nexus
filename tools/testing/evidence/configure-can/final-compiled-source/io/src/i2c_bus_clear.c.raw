/**
 * \file            i2c_bus_clear.c
 *
 * \brief           Explicit bounded I2C GPIO recovery and STOP settlement.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/i2c_bus_clear.h"

/** \brief Reject a clock-domain reversal before another external effect. */
static nx_result_t read_clock(nx_i2c_bus_clear_t* clear, nx_time_us_t* now) {
    *now = clear->clock.read(clear->clock.context);
    if (*now == NX_DEADLINE_NEVER ||
        (clear->clock_observed && *now < clear->last_time)) {
        return NX_ERROR_IO;
    }
    clear->last_time = *now;
    clear->clock_observed = true;
    return NX_SUCCESS;
}

/** \brief Each effect must remain inside its own finite absolute budget. */
static nx_result_t check_deadline(nx_i2c_bus_clear_t* clear,
                                  nx_time_us_t deadline) {
    nx_time_us_t now = 0;
    nx_result_t result = read_clock(clear, &now);
    if (result != NX_SUCCESS) {
        return result;
    }
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief Check both sides of a bounded open-drain operation. */
static nx_result_t drive(nx_i2c_bus_clear_t* clear, bool scl_low, bool sda_low,
                         nx_time_us_t deadline) {
    nx_result_t result = check_deadline(clear, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = clear->port.ops->drive(clear->port.context, scl_low, sda_low);
    return result == NX_SUCCESS ? check_deadline(clear, deadline) : result;
}

/** \brief Observe physical levels without qualifying late snapshots. */
static nx_result_t sample(nx_i2c_bus_clear_t* clear, bool* scl_high,
                          bool* sda_high, nx_time_us_t deadline) {
    nx_result_t result = check_deadline(clear, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = clear->port.ops->sample(clear->port.context, scl_high, sda_high);
    return result == NX_SUCCESS ? check_deadline(clear, deadline) : result;
}

/** \brief Saturate at the deadline and reject successful frozen-clock waits.
 */
static nx_result_t pause(nx_i2c_bus_clear_t* clear, uint32_t duration,
                         nx_time_us_t deadline) {
    nx_time_us_t now = 0;
    nx_result_t result = read_clock(clear, &now);
    if (result != NX_SUCCESS || now >= deadline) {
        return result == NX_SUCCESS ? NX_ERROR_TIMEOUT : result;
    }
    nx_time_us_t target =
        duration >= deadline - now ? deadline : now + duration;
    result = clear->port.ops->wait_until(clear->port.context, target);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = read_clock(clear, &now);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (now < target) {
        return NX_ERROR_IO;
    }
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief A released output latch does not establish SCL HIGH. */
static nx_result_t wait_scl(nx_i2c_bus_clear_t* clear, nx_time_us_t deadline,
                            bool* sda_high) {
    for (;;) {
        bool scl_high = false;
        nx_result_t result = sample(clear, &scl_high, sda_high, deadline);
        if (result != NX_SUCCESS || scl_high) {
            return result;
        }
        result = pause(clear, clear->timing.stretch_poll_us, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
}

/** \brief Count only completed, observed low/high recovery periods. */
static nx_result_t recover(nx_i2c_bus_clear_t* clear, nx_time_us_t deadline) {
    bool sda_high = false;
    nx_result_t result = wait_scl(clear, deadline, &sda_high);
    if (result != NX_SUCCESS || sda_high) {
        return result;
    }
    while (clear->result.pulses < NX_I2C_BUS_CLEAR_MAX_PULSES) {
        result = drive(clear, true, false, deadline);
        if (result == NX_SUCCESS) {
            result = pause(clear, clear->timing.low_us, deadline);
        }
        bool scl_high = true;
        if (result == NX_SUCCESS) {
            result = sample(clear, &scl_high, &sda_high, deadline);
            if (result == NX_SUCCESS && scl_high) {
                result = NX_ERROR_IO;
            }
        }
        if (result == NX_SUCCESS) {
            result = drive(clear, false, false, deadline);
        }
        if (result == NX_SUCCESS) {
            result = wait_scl(clear, deadline, &sda_high);
        }
        if (result == NX_SUCCESS) {
            result = pause(clear, clear->timing.high_us, deadline);
        }
        if (result == NX_SUCCESS) {
            result = sample(clear, &scl_high, &sda_high, deadline);
            if (result == NX_SUCCESS && !scl_high) {
                result = NX_ERROR_IO;
            }
        }
        if (result != NX_SUCCESS) {
            return result;
        }
        ++clear->result.pulses;
        if (sda_high) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief Establish SDA LOW under SCL LOW before constructing STOP. */
static nx_result_t stop(nx_i2c_bus_clear_t* clear, nx_time_us_t deadline) {
    nx_result_t result = drive(clear, true, false, deadline);
    if (result == NX_SUCCESS) {
        result = drive(clear, true, true, deadline);
    }
    if (result == NX_SUCCESS) {
        result = pause(clear, clear->timing.low_us, deadline);
    }
    bool scl_high = true;
    bool sda_high = true;
    if (result == NX_SUCCESS) {
        result = sample(clear, &scl_high, &sda_high, deadline);
        if (result == NX_SUCCESS && (scl_high || sda_high)) {
            result = NX_ERROR_IO;
        }
    }
    if (result == NX_SUCCESS) {
        result = drive(clear, false, true, deadline);
    }
    if (result == NX_SUCCESS) {
        result = wait_scl(clear, deadline, &sda_high);
        if (result == NX_SUCCESS && sda_high) {
            result = NX_ERROR_IO;
        }
    }
    if (result == NX_SUCCESS) {
        result = pause(clear, clear->timing.stop_setup_us, deadline);
    }
    if (result == NX_SUCCESS) {
        result = sample(clear, &scl_high, &sda_high, deadline);
        if (result == NX_SUCCESS && (!scl_high || sda_high)) {
            result = NX_ERROR_IO;
        }
    }
    if (result == NX_SUCCESS) {
        result = drive(clear, false, false, deadline);
    }
    if (result == NX_SUCCESS) {
        result = pause(clear, clear->timing.high_us, deadline);
    }
    if (result == NX_SUCCESS) {
        result = sample(clear, &scl_high, &sda_high, deadline);
        if (result == NX_SUCCESS && (!scl_high || !sda_high)) {
            result = NX_ERROR_IO;
        }
    }
    if (result == NX_SUCCESS) {
        clear->result.stop_observed = true;
    }
    return result;
}

/** \brief Retain the lease until STOP and actual ownership return succeed. */
static nx_result_t cleanup(nx_i2c_bus_clear_t* clear, nx_time_us_t deadline) {
    nx_result_t result = check_deadline(clear, deadline);
    if (result == NX_SUCCESS && !clear->result.stop_observed) {
        result = stop(clear, deadline);
    }
    if (result == NX_SUCCESS) {
        result = check_deadline(clear, deadline);
    }
    if (result == NX_SUCCESS) {
        result = clear->port.ops->release(clear->port.context);
        if (result == NX_SUCCESS) {
            /* A late successful handoff has no residual lease to quarantine.
             */
            clear->result.lease_held = false;
            result = check_deadline(clear, deadline);
        }
    }
    clear->result.cleanup = result;
    return result;
}

/** \brief Bind required methods without reading a clock or touching hardware.
 */
nx_result_t nx_i2c_bus_clear_init(nx_i2c_bus_clear_t* clear,
                                  nx_i2c_bus_clear_port_t port,
                                  nx_clock_t clock,
                                  nx_i2c_bus_clear_timing_t timing) {
    if (clear == NULL || port.ops == NULL || port.ops->acquire == NULL ||
        port.ops->drive == NULL || port.ops->sample == NULL ||
        port.ops->wait_until == NULL || port.ops->release == NULL ||
        clock.read == NULL || timing.low_us == 0 || timing.high_us == 0 ||
        timing.stop_setup_us == 0 || timing.stretch_poll_us == 0) {
        return NX_ERROR_INVALID;
    }
    if (clear->initialized && clear->result.lease_held) {
        return NX_ERROR_BUSY;
    }
    *clear = (nx_i2c_bus_clear_t){
        .port = port, .clock = clock, .timing = timing, .initialized = true};
    return NX_SUCCESS;
}

/** \brief One recovery attempt never borrows a transaction or replays data. */
nx_result_t nx_i2c_bus_clear_run(nx_i2c_bus_clear_t* clear,
                                 nx_time_us_t deadline,
                                 nx_time_us_t cleanup_deadline,
                                 nx_i2c_bus_clear_result_t* result) {
    if (clear == NULL || result == NULL || !clear->initialized ||
        deadline == NX_DEADLINE_NEVER ||
        cleanup_deadline == NX_DEADLINE_NEVER || cleanup_deadline < deadline) {
        return NX_ERROR_INVALID;
    }
    if (clear->result.lease_held) {
        return NX_ERROR_STATE;
    }
    clear->result = (nx_i2c_bus_clear_result_t){0};
    clear->result.primary = check_deadline(clear, deadline);
    if (clear->result.primary == NX_SUCCESS) {
        clear->result.primary = clear->port.ops->acquire(clear->port.context);
        if (clear->result.primary == NX_SUCCESS) {
            clear->result.lease_held = true;
            clear->result.primary = recover(clear, deadline);
            (void)cleanup(clear, cleanup_deadline);
        }
    }
    *result = clear->result;
    return result->primary != NX_SUCCESS ? result->primary : result->cleanup;
}

/** \brief Cleanup retries preserve the original primary and pulse history. */
nx_result_t nx_i2c_bus_clear_finish(nx_i2c_bus_clear_t* clear,
                                    nx_time_us_t deadline,
                                    nx_i2c_bus_clear_result_t* result) {
    if (clear == NULL || result == NULL || !clear->initialized ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (!clear->result.lease_held) {
        return NX_ERROR_STATE;
    }
    nx_result_t status = cleanup(clear, deadline);
    *result = clear->result;
    return status;
}

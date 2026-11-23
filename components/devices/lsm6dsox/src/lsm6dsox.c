/**
 * \file            lsm6dsox.c
 * \brief           LSM6DSOX official-driver bridge with explicit lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/lsm6dsox.h"
#include "lsm6dsox_reg.h"
#include <string.h>

static nx_result_t clock_check(nx_lsm6dsox_t* device) {
    const nx_time_us_t now =
        device->transport.clock.read(device->transport.clock.context);
    if (now == NX_DEADLINE_NEVER || now < device->last_clock) {
        device->time_fault = true;
        return NX_ERROR_IO;
    }
    device->last_clock = now;
    return now >= device->operation_deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

static int32_t read_register(void* context, uint8_t reg, uint8_t* data,
                             uint16_t bytes) {
    nx_lsm6dsox_t* device = context;
    if (device->error == NX_SUCCESS) {
        device->error = clock_check(device);
    }
    if (device->error == NX_SUCCESS) {
        device->error =
            device->transport.read(device->transport.context, reg, data, bytes,
                                   device->operation_deadline);
        const nx_result_t timing = clock_check(device);
        if (device->error == NX_SUCCESS) {
            device->error = timing;
        }
    }
    return device->error == NX_SUCCESS ? 0 : -1;
}

static int32_t write_register(void* context, uint8_t reg, const uint8_t* data,
                              uint16_t bytes) {
    nx_lsm6dsox_t* device = context;
    if (device->error == NX_SUCCESS) {
        device->error = clock_check(device);
    }
    if (device->error == NX_SUCCESS) {
        device->error =
            device->transport.write(device->transport.context, reg, data, bytes,
                                    device->operation_deadline);
        const nx_result_t timing = clock_check(device);
        if (device->error == NX_SUCCESS) {
            device->error = timing;
        }
    }
    return device->error == NX_SUCCESS ? 0 : -1;
}

static stmdev_ctx_t vendor_context(nx_lsm6dsox_t* device) {
    const stmdev_ctx_t context = {.write_reg = write_register,
                                  .read_reg = read_register,
                                  .handle = device};
    return context;
}

static nx_result_t enter(nx_lsm6dsox_t* device, nx_time_us_t deadline) {
    if (device == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call) {
        return NX_ERROR_BUSY;
    }
    if (!device->initialized || device->time_fault) {
        return NX_ERROR_STATE;
    }
    device->in_call = true;
    device->operation_deadline = deadline;
    device->error = clock_check(device);
    if (device->error != NX_SUCCESS) {
        device->in_call = false;
    }
    return device->error;
}

static nx_result_t leave(nx_lsm6dsox_t* device, nx_result_t result) {
    device->in_call = false;
    return result;
}

static nx_result_t failed(nx_lsm6dsox_t* device, nx_result_t result) {
    device->running = false;
    device->prepared = false;
    device->resetting = false;
    device->uncertain = true;
    return leave(device, result);
}

static nx_result_t vendor_result(const nx_lsm6dsox_t* device, int32_t result) {
    return result == 0
               ? NX_SUCCESS
               : (device->error == NX_SUCCESS ? NX_ERROR_IO : device->error);
}

static nx_io_timestamp_t observed(const nx_lsm6dsox_t* device) {
    const nx_io_timestamp_t timestamp = {
        .ticks = device->last_clock,
        .ticks_per_second = 1000000,
        .domain = device->config.clock_domain,
        .epoch = device->config.clock_epoch,
        .source = NX_IO_TIMESTAMP_SERVICE_OBSERVATION};
    return timestamp;
}

nx_result_t nx_lsm6dsox_init(nx_lsm6dsox_t* device,
                             nx_lsm6dsox_transport_port_t transport,
                             const nx_lsm6dsox_config_t* config,
                             nx_time_us_t deadline) {
    if (device == NULL || config == NULL || transport.context == NULL ||
        transport.read == NULL || transport.write == NULL ||
        transport.clock.read == NULL || config->clock_domain == 0 ||
        config->clock_epoch == 0 || deadline == NX_DEADLINE_NEVER ||
        (config->mode != NX_LSM6DSOX_DIRECT &&
         config->mode != NX_LSM6DSOX_TAGGED_FIFO)) {
        return NX_ERROR_INVALID;
    }
    if (device->in_call || device->initialized) {
        return NX_ERROR_BUSY;
    }
    device->in_call = true;
    nx_lsm6dsox_t candidate = {.transport = transport,
                               .config = *config,
                               .operation_deadline = deadline,
                               .in_call = true};
    const stmdev_ctx_t context = vendor_context(&candidate);
    uint8_t id = 0;
    const int32_t status = lsm6dsox_device_id_get(&context, &id);
    nx_result_t result = vendor_result(&candidate, status);
    if (result == NX_SUCCESS && id != LSM6DSOX_ID) {
        result = NX_ERROR_UNSUPPORTED;
    }
    if (result == NX_SUCCESS) {
        candidate.initialized = true;
        candidate.in_call = false;
        *device = candidate;
        return NX_SUCCESS;
    }
    device->in_call = false;
    return result;
}

nx_result_t nx_lsm6dsox_reset_begin(nx_lsm6dsox_t* device,
                                    nx_time_us_t deadline) {
    nx_result_t result = enter(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (device->running || device->resetting || device->uncertain) {
        return leave(device, NX_ERROR_STATE);
    }
    device->prepared = false;
    device->uncertain = true;
    const stmdev_ctx_t context = vendor_context(device);
    result = vendor_result(device, lsm6dsox_reset_set(&context, 1));
    if (result != NX_SUCCESS) {
        return failed(device, result);
    }
    device->reset_deadline = deadline;
    device->resetting = true;
    return leave(device, NX_SUCCESS);
}

nx_result_t nx_lsm6dsox_reset_poll(nx_lsm6dsox_t* device) {
    if (device == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter(device, device->reset_deadline);
    if (result != NX_SUCCESS) {
        return result == NX_ERROR_TIMEOUT ? failed(device, result) : result;
    }
    if (!device->resetting) {
        return leave(device, NX_ERROR_STATE);
    }
    const stmdev_ctx_t context = vendor_context(device);
    uint8_t pending = 1;
    result = vendor_result(device, lsm6dsox_reset_get(&context, &pending));
    if (result != NX_SUCCESS) {
        return failed(device, result);
    }
    if (pending != 0) {
        return leave(device, NX_ERROR_BUSY);
    }
    if (device->generation == UINT32_MAX) {
        return failed(device, NX_ERROR_EXHAUSTED);
    }
    ++device->generation;
    device->sequence = 0;
    device->resetting = false;
    device->uncertain = false;
    device->prepared = true;
    return leave(device, NX_SUCCESS);
}

static int32_t configure(const stmdev_ctx_t* context, nx_lsm6dsox_mode_t mode) {
    if (lsm6dsox_block_data_update_set(context, 1) != 0 ||
        lsm6dsox_auto_increment_set(context, 1) != 0 ||
        lsm6dsox_xl_full_scale_set(context, LSM6DSOX_4g) != 0 ||
        lsm6dsox_gy_full_scale_set(context, LSM6DSOX_500dps) != 0) {
        return -1;
    }
    if (mode == NX_LSM6DSOX_TAGGED_FIFO &&
        (lsm6dsox_timestamp_set(context, 1) != 0 ||
         lsm6dsox_fifo_timestamp_decimation_set(context, LSM6DSOX_DEC_1) != 0 ||
         lsm6dsox_fifo_xl_batch_set(context, LSM6DSOX_XL_BATCHED_AT_104Hz) !=
             0 ||
         lsm6dsox_fifo_gy_batch_set(context, LSM6DSOX_GY_BATCHED_AT_104Hz) !=
             0 ||
         lsm6dsox_fifo_mode_set(context, LSM6DSOX_STREAM_MODE) != 0)) {
        return -1;
    }
    return lsm6dsox_xl_data_rate_set(context, LSM6DSOX_XL_ODR_104Hz) != 0 ||
                   lsm6dsox_gy_data_rate_set(context, LSM6DSOX_GY_ODR_104Hz) !=
                       0
               ? -1
               : 0;
}

static int32_t verify_profile(const stmdev_ctx_t* context,
                              nx_lsm6dsox_mode_t mode) {
    lsm6dsox_odr_xl_t xl;
    lsm6dsox_odr_g_t gy;
    lsm6dsox_fs_xl_t xl_scale;
    lsm6dsox_fs_g_t gy_scale;
    lsm6dsox_fifo_mode_t fifo;
    uint8_t bdu = 0;
    uint8_t increment = 0;
    if (lsm6dsox_xl_data_rate_get(context, &xl) != 0 ||
        lsm6dsox_gy_data_rate_get(context, &gy) != 0 ||
        lsm6dsox_xl_full_scale_get(context, &xl_scale) != 0 ||
        lsm6dsox_gy_full_scale_get(context, &gy_scale) != 0 ||
        lsm6dsox_block_data_update_get(context, &bdu) != 0 ||
        lsm6dsox_auto_increment_get(context, &increment) != 0 ||
        lsm6dsox_fifo_mode_get(context, &fifo) != 0) {
        return -1;
    }
    if (xl != LSM6DSOX_XL_ODR_104Hz || gy != LSM6DSOX_GY_ODR_104Hz ||
        xl_scale != LSM6DSOX_4g || gy_scale != LSM6DSOX_500dps || bdu != 1 ||
        increment != 1 ||
        fifo != (mode == NX_LSM6DSOX_TAGGED_FIFO ? LSM6DSOX_STREAM_MODE
                                                 : LSM6DSOX_BYPASS_MODE)) {
        return -1;
    }
    if (mode == NX_LSM6DSOX_TAGGED_FIFO) {
        lsm6dsox_bdr_xl_t xl_batch;
        lsm6dsox_bdr_gy_t gy_batch;
        lsm6dsox_odr_ts_batch_t timestamp_batch;
        uint8_t timestamp = 0;
        if (lsm6dsox_fifo_xl_batch_get(context, &xl_batch) != 0 ||
            lsm6dsox_fifo_gy_batch_get(context, &gy_batch) != 0 ||
            lsm6dsox_fifo_timestamp_decimation_get(context, &timestamp_batch) !=
                0 ||
            lsm6dsox_timestamp_get(context, &timestamp) != 0 ||
            xl_batch != LSM6DSOX_XL_BATCHED_AT_104Hz ||
            gy_batch != LSM6DSOX_GY_BATCHED_AT_104Hz ||
            timestamp_batch != LSM6DSOX_DEC_1 || timestamp != 1) {
            return -1;
        }
    }
    return 0;
}

nx_result_t nx_lsm6dsox_start(nx_lsm6dsox_t* device, nx_time_us_t deadline) {
    nx_result_t result = enter(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!device->prepared || device->uncertain || device->running ||
        device->resetting) {
        return leave(device, NX_ERROR_STATE);
    }
    device->uncertain = true;
    const stmdev_ctx_t context = vendor_context(device);
    if (configure(&context, device->config.mode) != 0 ||
        verify_profile(&context, device->config.mode) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    device->running = true;
    device->uncertain = false;
    return leave(device, NX_SUCCESS);
}

static nx_result_t publication(nx_lsm6dsox_t* device, nx_lsm6dsox_mode_t mode) {
    if (!device->running || device->uncertain || device->config.mode != mode) {
        return NX_ERROR_STATE;
    }
    return device->sequence == UINT32_MAX ? NX_ERROR_EXHAUSTED : NX_SUCCESS;
}

nx_result_t nx_lsm6dsox_read(nx_lsm6dsox_t* device,
                             nx_lsm6dsox_sample_t* sample,
                             nx_time_us_t deadline) {
    if (sample == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = publication(device, NX_LSM6DSOX_DIRECT);
    if (result != NX_SUCCESS) {
        return leave(device, result);
    }
    const stmdev_ctx_t context = vendor_context(device);
    lsm6dsox_status_reg_t status;
    if (lsm6dsox_status_reg_get(&context, &status) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    if (status.xlda == 0 || status.gda == 0) {
        return leave(device, NX_ERROR_BUSY);
    }
    int16_t acceleration[3];
    int16_t angular_rate[3];
    if (lsm6dsox_acceleration_raw_get(&context, acceleration) != 0 ||
        lsm6dsox_angular_rate_raw_get(&context, angular_rate) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    nx_lsm6dsox_sample_t next = {.sequence = device->sequence + 1,
                                 .generation = device->generation,
                                 .observed = observed(device)};
    for (size_t axis = 0; axis < 3; ++axis) {
        next.acceleration_m_s2[axis] =
            lsm6dsox_from_fs4_to_mg(acceleration[axis]) * 0.00980665F;
        next.angular_rate_rad_s[axis] =
            lsm6dsox_from_fs500_to_mdps(angular_rate[axis]) *
            0.000017453292519943295F;
    }
    device->sequence = next.sequence;
    *sample = next;
    return leave(device, NX_SUCCESS);
}

static int16_t signed_word(const uint8_t* bytes) {
    const uint16_t value =
        (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
    return (int16_t)(value <= INT16_MAX ? (int32_t)value
                                        : (int32_t)value - 65536);
}

nx_result_t nx_lsm6dsox_fifo_pop(nx_lsm6dsox_t* device,
                                 nx_lsm6dsox_fifo_sample_t* sample,
                                 nx_time_us_t deadline) {
    if (sample == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = enter(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = publication(device, NX_LSM6DSOX_TAGGED_FIFO);
    if (result != NX_SUCCESS) {
        return leave(device, result);
    }
    const stmdev_ctx_t context = vendor_context(device);
    lsm6dsox_fifo_status2_t status;
    uint16_t count = 0;
    if (lsm6dsox_fifo_status_get(&context, &status) != 0 ||
        lsm6dsox_fifo_data_level_get(&context, &count) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    if (status.fifo_ovr_ia != 0 || status.over_run_latched != 0) {
        return failed(device, NX_ERROR_OVERFLOW);
    }
    if (count == 0) {
        return leave(device, NX_ERROR_EMPTY);
    }
    lsm6dsox_fifo_tag_t tag;
    uint8_t bytes[6];
    if (lsm6dsox_fifo_sensor_tag_get(&context, &tag) != 0 ||
        lsm6dsox_fifo_out_raw_get(&context, bytes) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    nx_lsm6dsox_fifo_sample_t next = {.sequence = device->sequence + 1,
                                      .generation = device->generation,
                                      .observed = observed(device)};
    if (tag == LSM6DSOX_TIMESTAMP_TAG) {
        next.kind = NX_LSM6DSOX_SENSOR_TIMESTAMP;
        next.sensor_ticks = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
                            ((uint32_t)bytes[2] << 16) |
                            ((uint32_t)bytes[3] << 24);
    } else if (tag == LSM6DSOX_XL_NC_TAG || tag == LSM6DSOX_GYRO_NC_TAG) {
        next.kind = tag == LSM6DSOX_XL_NC_TAG ? NX_LSM6DSOX_ACCELERATION
                                              : NX_LSM6DSOX_ANGULAR_RATE;
        for (size_t axis = 0; axis < 3; ++axis) {
            next.raw[axis] = signed_word(&bytes[2 * axis]);
            next.values[axis] =
                next.kind == NX_LSM6DSOX_ACCELERATION
                    ? lsm6dsox_from_fs4_to_mg(next.raw[axis]) * 0.00980665F
                    : lsm6dsox_from_fs500_to_mdps(next.raw[axis]) *
                          0.000017453292519943295F;
        }
    } else {
        return failed(device, NX_ERROR_UNSUPPORTED);
    }
    /* Recheck loss after the pop: a successful read cannot hide new overrun. */
    if (lsm6dsox_fifo_status_get(&context, &status) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    if (status.fifo_ovr_ia != 0 || status.over_run_latched != 0) {
        return failed(device, NX_ERROR_OVERFLOW);
    }
    next.observed = observed(device);
    device->sequence = next.sequence;
    *sample = next;
    return leave(device, NX_SUCCESS);
}

nx_result_t nx_lsm6dsox_stop(nx_lsm6dsox_t* device, nx_time_us_t deadline) {
    nx_result_t result = enter(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    const stmdev_ctx_t context = vendor_context(device);
    uint8_t pending = 1;
    if (lsm6dsox_reset_get(&context, &pending) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    if (pending != 0) {
        return leave(device, NX_ERROR_BUSY);
    }
    device->uncertain = true;
    if (lsm6dsox_xl_data_rate_set(&context, LSM6DSOX_XL_ODR_OFF) != 0 ||
        lsm6dsox_gy_data_rate_set(&context, LSM6DSOX_GY_ODR_OFF) != 0 ||
        lsm6dsox_fifo_mode_set(&context, LSM6DSOX_BYPASS_MODE) != 0) {
        return failed(device, vendor_result(device, -1));
    }
    lsm6dsox_odr_xl_t xl;
    lsm6dsox_odr_g_t gy;
    lsm6dsox_fifo_mode_t fifo;
    if (lsm6dsox_xl_data_rate_get(&context, &xl) != 0 ||
        lsm6dsox_gy_data_rate_get(&context, &gy) != 0 ||
        lsm6dsox_fifo_mode_get(&context, &fifo) != 0 ||
        xl != LSM6DSOX_XL_ODR_OFF || gy != LSM6DSOX_GY_ODR_OFF ||
        fifo != LSM6DSOX_BYPASS_MODE) {
        return failed(device, vendor_result(device, -1));
    }
    device->running = false;
    device->prepared = false;
    device->resetting = false;
    device->uncertain = false;
    return leave(device, NX_SUCCESS);
}

void nx_lsm6dsox_release(nx_lsm6dsox_t* device) {
    if (device != NULL && !device->in_call) {
        memset(device, 0, sizeof(*device));
    }
}

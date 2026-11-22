/**
 * \file            ov2640.c
 * \brief           Checked Nexus callbacks around the real ST sensor tables
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ov2640.h"
#include "ov2640.h"

/** \brief Transient upstream callback context; all entry is one task executor.
 */
static nx_ov2640_t* active;

/** \brief One original monotonic domain bounds every supplier callback. */
static nx_result_t observe(nx_ov2640_t* device) {
    if (device->error != NX_SUCCESS)
        return device->error;
    nx_time_us_t now =
        device->config->port.clock.read(device->config->port.clock.context);
    if (device->clock_failed || now == NX_DEADLINE_NEVER ||
        now < device->last_clock) {
        device->clock_failed = true;
        device->error = NX_ERROR_STATE;
    } else {
        device->last_clock = now;
        if (now >= device->deadline)
            device->error = NX_ERROR_TIMEOUT;
    }
    return device->error;
}

/** \brief The application has already established its physical SCCB owner. */
void CAMERA_IO_Init(void) {
}

void CAMERA_IO_Write(uint8_t address, uint8_t reg, uint8_t value) {
    if (active == NULL || observe(active) != NX_SUCCESS)
        return;
    if (address != 0x60U) {
        active->error = NX_ERROR_INVALID;
        return;
    }
    active->error = active->config->port.write(active->config->port.context,
                                               reg, value, active->deadline);
    (void)observe(active);
}

uint8_t CAMERA_IO_Read(uint8_t address, uint8_t reg) {
    if (active == NULL || observe(active) != NX_SUCCESS)
        return 0U;
    if (address != 0x60U) {
        active->error = NX_ERROR_INVALID;
        return 0U;
    }
    uint8_t value = 0U;
    active->error = active->config->port.read(active->config->port.context, reg,
                                              &value, active->deadline);
    (void)observe(active);
    return value;
}

void CAMERA_Delay(uint32_t milliseconds) {
    if (active == NULL || observe(active) != NX_SUCCESS)
        return;
    active->error = active->config->port.wait_ms(
        active->config->port.context, milliseconds, active->deadline);
    (void)observe(active);
}

/** \brief Detach transient callback context even after an upstream void error.
 */
static nx_result_t finish(nx_ov2640_t* device) {
    nx_result_t result = device->error;
    if (result != NX_SUCCESS) {
        device->initialized = false;
        device->uncertain = true;
    }
    active = NULL;
    device->in_call = false;
    return result;
}

nx_result_t nx_ov2640_initialize(nx_ov2640_t* device,
                                 const nx_ov2640_config_t* config,
                                 nx_time_us_t deadline) {
    if (device == NULL || config == NULL || config->port.write == NULL ||
        config->port.read == NULL || config->port.wait_ms == NULL ||
        config->port.clock.read == NULL || config->xclk_hz < 6000000U ||
        config->xclk_hz > 24000000U || config->expected_pid_l == 0U ||
        config->expected_pid_l == 0xffU ||
        config->resolution < NX_OV2640_QQVGA ||
        config->resolution > NX_OV2640_VGA || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (active != NULL || device->in_call || device->initialized ||
        (device->config != NULL && device->config != config))
        return NX_ERROR_BUSY;
    if (device->clock_failed)
        return NX_ERROR_STATE;
    nx_time_us_t now = config->port.clock.read(config->port.clock.context);
    if (now == NX_DEADLINE_NEVER ||
        (device->config != NULL && now < device->last_clock)) {
        if (device->config != NULL)
            device->clock_failed = true;
        return NX_ERROR_STATE;
    }
    if (now >= deadline)
        return NX_ERROR_TIMEOUT;
    *device = (nx_ov2640_t){.config = config,
                            .deadline = deadline,
                            .last_clock = now,
                            .uncertain = true,
                            .in_call = true};
    active = device;
    uint16_t identity = ov2640_ReadID(0x60U);
    uint8_t revision = CAMERA_IO_Read(0x60U, 0x0bU);
    if (device->error == NX_SUCCESS &&
        (identity != 0x26U || revision != config->expected_pid_l))
        device->error = NX_ERROR_UNSUPPORTED;
    if (device->error == NX_SUCCESS) {
        uint32_t resolution = CAMERA_R160x120;
        switch (config->resolution) {
            case NX_OV2640_UNKNOWN:
                device->error = NX_ERROR_INVALID;
                break;
            case NX_OV2640_QQVGA:
                break;
            case NX_OV2640_QVGA:
                resolution = CAMERA_R320x240;
                break;
            case NX_OV2640_480X272:
                resolution = CAMERA_R480x272;
                break;
            case NX_OV2640_VGA:
                resolution = CAMERA_R640x480;
                break;
        }
        if (device->error == NX_SUCCESS)
            ov2640_Init(0x60U, resolution);
    }
    if (device->error == NX_SUCCESS) {
        CAMERA_IO_Write(0x60U, 0xffU, 0U);
        uint8_t format = CAMERA_IO_Read(0x60U, 0xdaU);
        if (device->error == NX_SUCCESS && format != 0x09U)
            device->error = NX_ERROR_IO;
    }
    if (device->error == NX_SUCCESS) {
        device->initialized = true;
        device->uncertain = false;
    }
    return finish(device);
}

nx_result_t nx_ov2640_stop(nx_ov2640_t* device, nx_time_us_t deadline) {
    if (device == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (active != NULL || device->in_call)
        return NX_ERROR_BUSY;
    if (device->config == NULL)
        return NX_SUCCESS;
    if (device->clock_failed)
        return NX_ERROR_STATE;
    device->deadline = deadline;
    device->error = NX_SUCCESS;
    device->in_call = true;
    active = device;
    CAMERA_IO_Write(0x60U, 0xffU, 1U);
    uint8_t control = CAMERA_IO_Read(0x60U, 9U);
    CAMERA_IO_Write(0x60U, 9U, (uint8_t)(control | 0x10U));
    control = CAMERA_IO_Read(0x60U, 9U);
    if (device->error == NX_SUCCESS && (control & 0x10U) == 0U)
        device->error = NX_ERROR_IO;
    nx_result_t result = finish(device);
    if (result == NX_SUCCESS)
        *device = (nx_ov2640_t){0};
    return result;
}

/**
 * \file            services.c
 * \brief           Installed TMP117 composition over a settled software bus
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/tmp117_i2c.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"

/** \brief Consumer-owned endpoint state; no physical sensor is emulated by IO.
 */
typedef struct {
    uint16_t configuration;
    nx_time_us_t ticks;
} model_t;

static model_t model;
static nx_tmp117_t device;
static nx_tmp117_sample_t sample;

/** \brief Explicit monotonic software domain, independent from MCU time. */
static nx_time_us_t clock_read(void* context) {
    model_t* state = context;
    return ++state->ticks;
}

/** \brief Every software transaction settles all local message spans. */
static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred) {
    model_t* state = context;
    *transferred = 0U;
    if (deadline == NX_DEADLINE_NEVER || messages == NULL || count == 0U ||
        messages[0].read || messages[0].data == NULL ||
        messages[0].length == 0U) {
        return NX_ERROR_INVALID;
    }
    uint8_t reg = messages[0].data[0];
    if (count == 1U && messages[0].length == 3U && reg == 1U) {
        state->configuration = (uint16_t)((uint16_t)messages[0].data[1] << 8U |
                                          messages[0].data[2]);
        if ((state->configuration & 0x0c00U) == 0x0c00U) {
            state->configuration |= 0x2000U;
        }
        *transferred = 3U;
        return NX_SUCCESS;
    }
    if (count != 2U || messages[0].length != 1U || !messages[1].read ||
        messages[1].length != 2U || messages[1].data == NULL) {
        return NX_ERROR_INVALID;
    }
    uint16_t value;
    switch (reg) {
        case 0U:
            value = 0x0c80U; /* Software reference: exactly 25 Celsius. */
            break;
        case 1U:
            value = state->configuration;
            state->configuration &= (uint16_t)~0x2000U;
            break;
        case 4U:
        case 7U:
            value = 0U;
            break;
        case 15U:
            value = 0x0117U;
            break;
        default:
            return NX_ERROR_UNSUPPORTED;
    }
    messages[1].data[0] = (uint8_t)(value >> 8U);
    messages[1].data[1] = (uint8_t)value;
    *transferred = 3U;
    return NX_SUCCESS;
}

static const nx_i2c_endpoint_ops_t operations = {.transaction = transaction};
static const nx_i2c_endpoint_t endpoint = {&operations, &model};

int main(void) {
    nx_platform_start_result_t boot = nx_platform_start();
    if (boot.primary != NX_SUCCESS || boot.cleanup != NX_SUCCESS ||
        boot.remaining_effects != 0U ||
        nx_factory_gpio(NX_GPIO_ID_LED0) == NULL) {
        return 1;
    }
    const nx_tmp117_config_t config = {1U, 1U, 0};
    nx_tmp117_transport_port_t transport =
        nx_tmp117_i2c_transport(&endpoint, (nx_clock_t){clock_read, &model});
    nx_result_t result = nx_tmp117_init(&device, transport, &config, 1000U);
    if (result == NX_SUCCESS) {
        result = nx_tmp117_begin(&device, 2000U);
    }
    if (result == NX_SUCCESS) {
        result = nx_tmp117_poll(&device, &sample);
    }
    bool valid = result == NX_SUCCESS && sample.micro_celsius == 25000000 &&
                 sample.raw_lsb == 3200 && sample.offset_lsb == 0 &&
                 sample.observed.domain == 1U && sample.observed.epoch == 1U &&
                 sample.observed.ticks >= sample.conversion_started_at;
    nx_result_t cleanup = nx_tmp117_cancel(&device, 3000U);
    nx_tmp117_stop(&device);
    nx_result_t stopped = nx_platform_stop();
    return valid && cleanup == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}

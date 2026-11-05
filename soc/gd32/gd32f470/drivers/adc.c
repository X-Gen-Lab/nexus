/**
 * \file            adc.c
 * \brief           GD32 ADC0 calibrated single-shot and sequential polling scan
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "nexus/core/fault.h"
#include "private/system.h"

static nx_gd32_adc_state_t* s_adc;

/** \brief Only the mode and pull fields touched by analog pin selection. */
typedef struct {
    uint32_t fields;
    uint32_t mode;
    uint32_t pull;
} adc_pin_snapshot_t;

/** \brief Retain the constructor frame and remaining effects after bad cleanup.
 */
static _Noreturn void rollback_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

/** \brief Return rejection only after every selected restore is observed. */
static void rollback(const adc_pin_snapshot_t pins[3], uint32_t added_clocks,
                     uint32_t common) {
    ADC_CTL1(ADC0) = 0u;
    nx_gd32_peripheral_barrier();
    if (ADC_CTL1(ADC0) != 0u) {
        rollback_park();
    }
    ADC_SYNCCTL = common;
    nx_gd32_peripheral_barrier();
    if (ADC_SYNCCTL != common) {
        rollback_park();
    }
    for (unsigned index = 0u; index < 3u; ++index) {
        uint32_t fields = pins[index].fields;
        if (fields != 0u) {
            uint32_t gpio = GPIOA + index * 0x400u;
            GPIO_PUD(gpio) = (GPIO_PUD(gpio) & ~fields) | pins[index].pull;
            GPIO_CTL(gpio) = (GPIO_CTL(gpio) & ~fields) | pins[index].mode;
            nx_gd32_peripheral_barrier();
            if ((GPIO_PUD(gpio) & fields) != pins[index].pull ||
                (GPIO_CTL(gpio) & fields) != pins[index].mode) {
                rollback_park();
            }
        }
    }
    /* Keep both ADC and GPIO powered until conversion/common/pads are proven
     * restored. Never invalidate a required readback by dropping its gate.
     */
    rcu_periph_clock_disable(RCU_ADC0);
    nx_gd32_peripheral_barrier();
    if ((RCU_APB2EN & RCU_APB2EN_ADC0EN) != 0u) {
        rollback_park();
    }
    RCU_AHB1EN &= ~added_clocks;
    nx_gd32_peripheral_barrier();
    if ((RCU_AHB1EN & added_clocks) != 0u) {
        rollback_park();
    }
}

/** \brief           Bound calibration waits without the SDK's infinite loop. */
static nx_result_t wait_clear(uint32_t mask, nx_time_us_t deadline) {
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        if ((ADC_CTL1(ADC0) & mask) == 0u) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief Errata Rev1.6 2.4.3 requires two ADC clocks after observed EOC. */
static nx_result_t wait_result(nx_time_us_t deadline) {
    /* APB2/8 is 12.5 MHz. Two 1 us timer ticks guarantee at least 1 us
     * despite counter quantization, exceeding the required 160 ns. Do not
     * infer the delay from incidental SDK or deadline-call instruction cost.
     */
    nx_time_us_t ready = nx_deadline_after(nx_time_now_us(), 2u);
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        nx_time_us_t now = nx_time_now_us();
        if (nx_deadline_expired(deadline, now)) {
            return NX_ERROR_TIMEOUT;
        }
        if (nx_deadline_expired(ready, now)) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief Every power-on needs stabilization and bounded calibration again. */
static nx_result_t power_on(nx_time_us_t deadline) {
    ADC_CTL1(ADC0) = ADC_CTL1_ADCON;
    nx_gd32_peripheral_barrier();
    (void)ADC_CTL1(ADC0);
    nx_time_us_t stable = nx_deadline_after(nx_time_now_us(), 10u);
    uint32_t polls = 0u;
    while (!nx_deadline_expired(stable, nx_time_now_us())) {
        if (++polls == 1000000u) {
            return NX_ERROR_IO;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
    }
    ADC_CTL1(ADC0) |= ADC_CTL1_RSTCLB;
    nx_gd32_peripheral_barrier();
    nx_result_t status = wait_clear(ADC_CTL1_RSTCLB, deadline);
    if (status == NX_SUCCESS) {
        ADC_CTL1(ADC0) |= ADC_CTL1_CLB;
        nx_gd32_peripheral_barrier();
        status = wait_clear(ADC_CTL1_CLB, deadline);
    }
    return status;
}

/** \brief Acquire one shared ADC domain with the selected cold clock. */
static nx_result_t initialize(nx_gd32_adc_state_t* port,
                              const uint8_t* channels, size_t count,
                              uint32_t reference_mv, nx_time_us_t deadline,
                              uint32_t clock) {
    if (!port || !channels || !count || count > 16u || reference_mv < 2600u ||
        reference_mv > 3600u) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0u; i < count; ++i) {
        if (channels[i] > 15u) {
            return NX_ERROR_INVALID;
        }
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    /* The SDK reset and prescaler affect all ADCs. Clock-gated external
     * owners must also remain quiescent under the exclusive startup contract.
     */
    if (s_adc || (RCU_APB2EN & (RCU_APB2EN_ADC0EN | RCU_APB2EN_ADC1EN |
                                RCU_APB2EN_ADC2EN)) != 0u) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    adc_pin_snapshot_t pins[3] = {0};
    uint32_t clock_mask = 0u;
    for (size_t i = 0u; i < count; ++i) {
        unsigned channel = channels[i];
        unsigned index = channel < 8u ? 0u : channel < 10u ? 1u : 2u;
        unsigned pin = channel < 8u    ? channel
                       : channel < 10u ? channel - 8u
                                       : channel - 10u;
        pins[index].fields |= 3u << (pin * 2u);
        clock_mask |= UINT32_C(1) << index;
    }
    uint32_t added_clocks = clock_mask & ~RCU_AHB1EN;
    RCU_AHB1EN |= clock_mask;
    (void)RCU_AHB1EN;
    nx_gd32_peripheral_barrier();
    /* GPIO register values cannot be captured while their clock is gated.
     * Capture every selected field once, including duplicate scan channels.
     */
    for (unsigned index = 0u; index < 3u; ++index) {
        if (pins[index].fields != 0u) {
            uint32_t gpio = GPIOA + index * 0x400u;
            pins[index].mode = GPIO_CTL(gpio) & pins[index].fields;
            pins[index].pull = GPIO_PUD(gpio) & pins[index].fields;
        }
    }
    for (size_t i = 0u; i < count; ++i) {
        unsigned channel = channels[i];
        unsigned index = channel < 8u ? 0u : channel < 10u ? 1u : 2u;
        unsigned pin = channel < 8u    ? channel
                       : channel < 10u ? channel - 8u
                                       : channel - 10u;
        gpio_mode_set(GPIOA + index * 0x400u, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                      UINT32_C(1) << pin);
    }
    rcu_periph_clock_enable(RCU_ADC0);
    (void)RCU_APB2EN;
    nx_gd32_peripheral_barrier();
    uint32_t common = ADC_SYNCCTL;
    adc_deinit();
    adc_clock_config(clock);
    nx_gd32_peripheral_barrier();
    if ((ADC_SYNCCTL & ADC_SYNCCTL_ADCCK) != clock) {
        rollback(pins, added_clocks, common);
        return NX_ERROR_IO;
    }
    ADC_CTL0(ADC0) = 0u;
    nx_result_t status = power_on(deadline);
    if (status != NX_SUCCESS) {
        rollback(pins, added_clocks, common);
        return status;
    }
    *port = (nx_gd32_adc_state_t){.channels = channels,
                                  .channel_count = count,
                                  .reference_mv = reference_mv,
                                  .initialized = true};
    s_adc = port;
    return NX_SUCCESS;
}

/** \brief Bind polling channels with the reviewed 12.5 MHz ADC clock. */
nx_result_t nx_gd32_adc_initialize(nx_gd32_adc_state_t* port,
                                   const uint8_t* channels, size_t count,
                                   uint32_t reference_mv,
                                   nx_time_us_t deadline) {
    return initialize(port, channels, count, reference_mv, deadline,
                      ADC_ADCCK_PCLK2_DIV8);
}

/** \brief HCLK/6 avoids errata 2.4.3 for autonomous DMA data reads. */
nx_result_t nx_gd32_adc_dma_initialize(nx_gd32_adc_state_t* port,
                                       const uint8_t* channels, size_t count,
                                       uint32_t reference_mv,
                                       nx_time_us_t deadline) {
    /* Fixed HCLK200/APB2 100 MHz: ADC is 100/3 MHz, above APB2/4 and below
     * the datasheet's 40 MHz maximum. Choose before power-on/calibration;
     * the shared owner excludes incompatible polling or another ADC domain.
     */
    return initialize(port, channels, count, reference_mv, deadline,
                      ADC_ADCCK_HCLK_DIV6);
}

/** \brief           Return raw count format without inventing calibrated volts.
 */
nx_result_t nx_gd32_adc_info(const void* context, nx_adc_info_t* info) {
    const nx_gd32_adc_state_t* port = context;
    if (!port || port != s_adc || !port->initialized || !info) {
        return NX_ERROR_INVALID;
    }
    *info = (nx_adc_info_t){12u, port->reference_mv, port->channel_count};
    return NX_SUCCESS;
}

/** \brief           Run one conversion per sequence element with valid prefix.
 */
nx_result_t nx_gd32_adc_sample(void* context, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count) {
    nx_gd32_adc_state_t* port = context;
    if (!port || port != s_adc || !port->initialized || !samples || !count ||
        capacity < port->channel_count) {
        return NX_ERROR_INVALID;
    }
    *count = 0u;
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    port->active = true;
    nx_result_t status = NX_SUCCESS;
    if ((ADC_CTL1(ADC0) & ADC_CTL1_ADCON) == 0u) {
        status = power_on(deadline);
    }
    for (size_t i = 0u; status == NX_SUCCESS && i < port->channel_count; ++i) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
        unsigned channel = port->channels[i];
        ADC_RSQ0(ADC0) = 0u;
        ADC_RSQ1(ADC0) = 0u;
        ADC_RSQ2(ADC0) = channel;
        if (channel < 10u) {
            ADC_SAMPT1(ADC0) |= 7u << (channel * 3u);
        } else {
            ADC_SAMPT0(ADC0) |= 7u << ((channel - 10u) * 3u);
        }
        ADC_STAT(ADC0) = 0u;
        ADC_CTL1(ADC0) |= ADC_CTL1_ADCON | ADC_CTL1_SWRCST;
        for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
            uint32_t flags = ADC_STAT(ADC0);
            if ((flags & ADC_STAT_ROVF) != 0u) {
                status = NX_ERROR_IO;
                break;
            }
            if (nx_deadline_expired(deadline, nx_time_now_us())) {
                status = NX_ERROR_TIMEOUT;
                break;
            }
            if ((flags & ADC_STAT_EOC) != 0u) {
                status = wait_result(deadline);
                if (status == NX_SUCCESS) {
                    samples[(*count)++] =
                        (uint16_t)(adc_routine_data_read(ADC0) & 0xFFFu);
                }
                break;
            }
            if (polls + 1u == 1000000u) {
                status = NX_ERROR_IO;
            }
        }
        if (status != NX_SUCCESS) {
            break;
        }
    }
    if (status != NX_SUCCESS) {
        ADC_CTL1(ADC0) = 0u;
        nx_gd32_peripheral_barrier();
        ADC_STAT(ADC0) = 0u;
    }
    port->active = false;
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Disable conversions before releasing configuration storage.
 */
nx_result_t nx_gd32_adc_stop(nx_gd32_adc_state_t* port) {
    if (!port || port != s_adc || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    ADC_CTL1(ADC0) = 0u;
    nx_gd32_peripheral_barrier();
    if (ADC_CTL1(ADC0) != 0u) {
        return NX_ERROR_IO;
    }
    ADC_STAT(ADC0) = 0u;
    rcu_periph_clock_disable(RCU_ADC0);
    nx_gd32_peripheral_barrier();
    if ((RCU_APB2EN & RCU_APB2EN_ADC0EN) != 0u) {
        return NX_ERROR_IO;
    }
    port->initialized = false;
    s_adc = NULL;
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_adc_ops_t nx_gd32_adc_ops = {
    .info = nx_gd32_adc_info,
    .sample = nx_gd32_adc_sample,
};

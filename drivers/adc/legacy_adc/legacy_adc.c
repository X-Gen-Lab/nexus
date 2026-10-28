/**
 * \file            legacy_adc.c
 * \brief           Reviewed single PA0 legacy ADC calibration and shots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "legacy_adc.h"
#define ADC_POWER             1U
#define ADC_CALIBRATE         4U
#define ADC_RESET_CALIBRATION 8U
#define ADC_SOFTWARE          ((7U << 17U) | (1U << 20U))
#define ADC_START             (1U << 22U)
#define ADC_EOC               2U

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool valid(const nx_legacy_adc_state_t* p) {
    if (p == NULL || p->config == NULL ||
        (uintptr_t)p->config > UINTPTR_MAX - sizeof(*p->config) ||
        (uintptr_t)p->config % _Alignof(nx_legacy_adc_config_t) != 0U ||
        overlap(p, sizeof(*p), p->config, sizeof(*p->config)))
        return false;
    return p->config->registers != NULL && p->config->gate != NULL &&
           p->config->divider != NULL && p->config->pin_mode != NULL &&
           p->config->pin_lock != NULL && p->config->clock_started != NULL &&
           p->config->gate_mask != 0U && p->reference_mv >= 2700U &&
           p->reference_mv <= 3600U && p->config->poll_limit != 0U &&
           p->config->startup_delay_us >= 4U &&
           p->config->startup_delay_us <= 2000U;
}
static bool clock_ready(const nx_legacy_adc_state_t* p) {
    return *p->config->clock_started &&
           (NX_LEGACY_READ(p->config->gate) & p->config->gate_mask) ==
               p->config->gate_mask &&
           (NX_LEGACY_READ(p->config->divider) & p->config->divider_mask) == 0U;
}
static bool write_checked(volatile uint32_t* r, uint32_t value) {
    NX_LEGACY_WRITE(r, value);
    nx_arch_dsb();
    return NX_LEGACY_READ(r) == value;
}
static bool alias(const nx_legacy_adc_state_t* p, const void* ptr,
                  size_t size) {
    return ptr == NULL || (uintptr_t)ptr > UINTPTR_MAX - size ||
           overlap(ptr, size, p, sizeof(*p)) ||
           overlap(ptr, size, p->config, sizeof(*p->config)) ||
           overlap(ptr, size, p->config->registers,
                   sizeof(*p->config->registers)) ||
           overlap(ptr, size, (const void*)p->config->gate,
                   sizeof(*p->config->gate)) ||
           overlap(ptr, size, (const void*)p->config->divider,
                   sizeof(*p->config->divider)) ||
           overlap(ptr, size, (const void*)p->config->pin_mode,
                   sizeof(*p->config->pin_mode)) ||
           overlap(ptr, size, (const void*)p->config->pin_lock,
                   sizeof(*p->config->pin_lock)) ||
           overlap(ptr, size, (const void*)p->config->clock_started,
                   sizeof(*p->config->clock_started));
}
/** \brief Bounded calibration waits never assume a progressing converter. */
static nx_result_t calibration(nx_legacy_adc_state_t* p, uint32_t command) {
    NX_LEGACY_WRITE(&p->config->registers->control2, ADC_POWER | command);
    nx_arch_dsb();
    uint32_t remaining = p->config->poll_limit;
    do {
        NX_LEGACY_POLL(1U, p);
        if ((NX_LEGACY_READ(&p->config->registers->control2) & command) == 0U)
            return NX_SUCCESS;
    } while (--remaining != 0U);
    return NX_ERROR_TIMEOUT;
}
nx_result_t nx_legacy_adc_initialize(nx_legacy_adc_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    nx_legacy_adc_registers_t* r = p->config->registers;
    uint32_t pin_mode = NX_LEGACY_READ(p->config->pin_mode) & 15U;
    if (p->retained || p->initialized || p->active ||
        NX_LEGACY_READ(&r->control1) != 0U ||
        NX_LEGACY_READ(&r->control2) != 0U ||
        NX_LEGACY_READ(&r->sequence1) != 0U ||
        NX_LEGACY_READ(&r->sequence2) != 0U ||
        NX_LEGACY_READ(&r->sequence3) != 0U ||
        (NX_LEGACY_READ(p->config->pin_lock) & 1U) != 0U ||
        (pin_mode != 0U && pin_mode != 4U))
        return NX_ERROR_BUSY;
    p->retained = true;
    p->fault = false;
    if (!write_checked(p->config->pin_mode,
                       NX_LEGACY_READ(p->config->pin_mode) & ~15U) ||
        !write_checked(&r->sample2, (NX_LEGACY_READ(&r->sample2) & ~7U) | 7U) ||
        !write_checked(&r->control2, ADC_POWER))
        return NX_ERROR_IO;
    /* F103 uses4us; GD303 uses2ms nominal for errataRev1.4 ADC2.4.1's1ms
     * minimum, with margin for the reviewed IRC8 factory-trim tolerance.
     * Supply decoupling and bead workarounds remain Board responsibilities. */
    nx_time_us_t stable =
        nx_deadline_after(nx_time_now_us(), p->config->startup_delay_us);
    uint32_t remaining = p->config->poll_limit;
    while (!nx_deadline_expired(stable, nx_time_now_us())) {
        NX_LEGACY_POLL(1U, p);
        if (--remaining == 0U)
            return NX_ERROR_TIMEOUT;
    }
    nx_result_t result = calibration(p, ADC_RESET_CALIBRATION);
    if (result == NX_SUCCESS)
        result = calibration(p, ADC_CALIBRATE);
    if (result == NX_SUCCESS)
        p->initialized = true;
    return result;
}
nx_result_t nx_legacy_adc_close(nx_legacy_adc_state_t* p) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->active)
        return NX_ERROR_BUSY;
    if (!clock_ready(p))
        return NX_ERROR_STATE;
    if (!write_checked(&p->config->registers->control2, 0U) ||
        !write_checked(&p->config->registers->control1, 0U))
        return NX_ERROR_IO;
    NX_LEGACY_WRITE(&p->config->registers->status, 0U);
    p->initialized = false;
    p->retained = false;
    p->fault = false;
    return NX_SUCCESS;
}
static nx_result_t info(const void* context, nx_adc_info_t* out) {
    const nx_legacy_adc_state_t* p = context;
    if (!valid(p) || out == NULL || alias(p, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    if (!p->initialized)
        return NX_ERROR_STATE;
    *out = (nx_adc_info_t){12U, p->reference_mv, 1U};
    return NX_SUCCESS;
}
static const uint8_t channel[] = {0U};
static const uint8_t sample[] = {7U};
static const uint16_t cycles[] = {2U, 8U, 14U, 29U, 42U, 56U, 72U, 240U};
static nx_result_t sequence(const void* context, nx_adc_sequence_info_t* out) {
    const nx_legacy_adc_state_t* p = context;
    if (!valid(p) || out == NULL || alias(p, out, sizeof(*out)))
        return NX_ERROR_INVALID;
    if (!p->initialized)
        return NX_ERROR_STATE;
    /* Integer API uses conservative ceiling sample/conversion counts.
     * The physical sum is252cycles; scheduling bound253 includes both
     * rounded components and is not a sample-aperture timestamp. */
    *out = (nx_adc_sequence_info_t){channel,  sample, cycles, 1U, 8U,
                                    4000000U, 0U,     253U,   13U};
    return NX_SUCCESS;
}
static nx_result_t shot(void* context, uint16_t* output, size_t capacity,
                        nx_time_us_t deadline, size_t* count) {
    nx_legacy_adc_state_t* p = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || capacity == 0U ||
        (uintptr_t)output % _Alignof(uint16_t) != 0U ||
        (uintptr_t)count % _Alignof(size_t) != 0U ||
        alias(p, output, sizeof(*output)) || alias(p, count, sizeof(*count)) ||
        overlap(output, sizeof(*output), count, sizeof(*count)))
        return NX_ERROR_INVALID;
    if (!p->initialized || p->fault || !clock_ready(p))
        return NX_ERROR_STATE;
    if (p->active)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    *count = 0U;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_legacy_adc_registers_t* r = p->config->registers;
    if (NX_LEGACY_READ(&r->control1) != 0U ||
        NX_LEGACY_READ(&r->control2) != ADC_POWER ||
        NX_LEGACY_READ(&r->sequence1) != 0U ||
        NX_LEGACY_READ(&r->sequence2) != 0U ||
        NX_LEGACY_READ(&r->sequence3) != 0U ||
        (NX_LEGACY_READ(&r->sample2) & 7U) != 7U)
        return NX_ERROR_STATE;
    p->active = true;
    NX_LEGACY_WRITE(&r->status, 0U);
    NX_LEGACY_WRITE(&r->control2, ADC_POWER | ADC_SOFTWARE | ADC_START);
    nx_arch_dsb();
    nx_result_t result = NX_SUCCESS;
    while ((NX_LEGACY_READ(&r->status) & ADC_EOC) == 0U) {
        NX_LEGACY_POLL(1U, p);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
    }
    uint16_t value = 0U;
    if (result == NX_SUCCESS)
        value = (uint16_t)(NX_LEGACY_READ(&r->data) & 0xfffU);
    /* No DMA or output address ever reaches hardware. On failed withdrawal,
     * retain converter ownership; caller buffers still end at this return. */
    if (!write_checked(&r->control2, ADC_POWER)) {
        p->fault = true;
        result = NX_ERROR_IO;
    }
    NX_LEGACY_WRITE(&r->status, 0U);
    if (result == NX_SUCCESS && nx_deadline_expired(deadline, nx_time_now_us()))
        result = NX_ERROR_TIMEOUT;
    if (result == NX_SUCCESS) {
        *output = value;
        *count = 1U;
    }
    p->active = false;
    return result;
}
const nx_adc_ops_t nx_legacy_adc_ops = {
    .info = info, .sample = shot, .sequence_info = sequence};

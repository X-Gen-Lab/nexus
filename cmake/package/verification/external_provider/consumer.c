/**
 * \file            consumer.c
 * \brief           C11 only installed SDK lifecycle acceptance reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#if __has_include("lifecycle_adapter.h") ||                                    \
                  __has_include("provider.h") ||                               \
                                __has_include("stm32f4xx.h") ||                \
                                              __has_include("gd32f4xx.h")
#error Private provider or vendor headers leaked into the public consumer
#endif
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "nexus_reference/lifecycle_probe.h"

/** \brief Exercise stable public faces and actual C rollback without casts. */
static int lifecycle_cycle(void) {
    nx_reference_reset();
    const nx_gpio_port_t* alpha = nx_factory_gpio_ref(NX_GPIO_REF_ALPHA);
    const nx_gpio_port_t* beta = nx_factory_gpio_ref(NX_GPIO_REF_BETA);
    if (alpha == NULL || beta == NULL || alpha == beta ||
        alpha->context == beta->context || alpha->ops != beta->ops ||
        nx_reference_event_count() != 0U ||
        nx_gpio_port_write(alpha, 1U, 0U) != NX_ERROR_INVALID) {
        return 10;
    }
    if (nx_platform_start().primary != NX_SUCCESS ||
        nx_gpio_port_write(alpha, 1U, 0U) != NX_SUCCESS ||
        nx_native_gpio_model_output(alpha) != 1U ||
        nx_native_gpio_model_output(beta) != 0U ||
        nx_gpio_port_write(beta, 8U, 0U) != NX_SUCCESS ||
        nx_gpio_port_toggle(alpha, 1U) != NX_SUCCESS ||
        nx_native_gpio_model_output(alpha) != 0U ||
        nx_native_gpio_model_output(beta) != 8U ||
        nx_platform_stop() != NX_SUCCESS) {
        return 20;
    }
    nx_reference_reset();
    nx_reference_fail_prepare(8U);
    nx_platform_diagnostic_t diagnostic;
    const nx_platform_start_result_t failed =
        nx_platform_start_diagnosed(&diagnostic);
    if (failed.primary != NX_ERROR_IO || failed.cleanup != NX_SUCCESS ||
        failed.remaining_effects != 0U ||
        diagnostic.controller_index != NX_PLATFORM_CONTROLLER_INDEX_GPIO_BETA ||
        nx_reference_event_count() != 3U ||
        nx_reference_event(0U).kind != NX_REFERENCE_PREPARE ||
        nx_reference_event(0U).mask != 1U ||
        nx_reference_event(1U).kind != NX_REFERENCE_PREPARE ||
        nx_reference_event(1U).mask != 8U ||
        nx_reference_event(2U).kind != NX_REFERENCE_STOP ||
        nx_reference_event(2U).mask != 1U ||
        nx_gpio_port_write(alpha, 1U, 0U) != NX_ERROR_INVALID ||
        nx_gpio_port_write(beta, 8U, 0U) != NX_ERROR_INVALID) {
        return 30;
    }
    nx_reference_fail_prepare(0U);
    if (nx_platform_start().primary != NX_SUCCESS ||
        nx_factory_gpio_ref(NX_GPIO_REF_ALPHA) != alpha ||
        nx_factory_gpio_ref(NX_GPIO_REF_BETA) != beta ||
        nx_gpio_port_write(alpha, 1U, 0U) != NX_SUCCESS ||
        nx_gpio_port_write(beta, 8U, 0U) != NX_SUCCESS ||
        nx_platform_stop() != NX_SUCCESS || nx_reference_event_count() != 7U) {
        return 40;
    }
    return 0;
}

/** \brief Repeat the public C integration without framework or heap linkage. */
int main(void) {
    for (unsigned cycle = 0U; cycle < 4U; ++cycle) {
        const int result = lifecycle_cycle();
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

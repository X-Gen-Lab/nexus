/**
 * \file            main.c
 * \brief           Exercise F1 device faces through the public firmware graph
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory_strong.h"

#if defined(NEXUS_BACKEND_FREERTOS)
#include "nexus/os/storage.h"
#endif

/** \brief Keep TX storage alive even if the provider cannot settle it. */
static nx_result_t exercise(void) {
    static const uint8_t message[] = {'N', 'X'};
    static nx_uart_tx_request_t request;
    const nx_gpio_signal_t* status =
        nx_factory_gpio_signal_ref(NX_GPIO_REF_STATUS);
    const nx_uart_port_t* console = nx_factory_uart_ref(NX_UART_REF_CONSOLE);

    nx_result_t result = nx_gpio_signal_write(status, true);
    if (result != NX_SUCCESS) {
        return result;
    }
#if defined(NX_GPIO_REF_SENSE)
    bool sense;
    result = nx_gpio_signal_read(nx_factory_gpio_signal_ref(NX_GPIO_REF_SENSE),
                                 &sense);
    if (result != NX_SUCCESS) {
        return result;
    }
#endif
    uint8_t byte;
    size_t received;
    result = nx_uart_port_read_bytes(console, &byte, sizeof(byte), &received);
    if (result != NX_SUCCESS && result != NX_ERROR_EMPTY) {
        return result;
    }

    nx_request_initialize(&request.base);
    const nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 100000U);
    result = nx_uart_tx_prepare(&request, message, sizeof(message), deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_uart_port_submit(console, &request);
    if (result != NX_SUCCESS) {
        return result;
    }

    for (;;) {
        nx_uart_port_service(console);
        size_t transferred;
        const nx_result_t observed =
            nx_request_result(&request.base, &result, &transferred);
        if (observed == NX_SUCCESS) {
            if (result != NX_SUCCESS) {
                return result;
            }
            if (transferred != sizeof(message)) {
                return NX_ERROR_IO;
            }
            return nx_gpio_signal_write(status, false);
        }
        if (observed != NX_ERROR_BUSY) {
            return observed;
        }
        if (nx_time_now_us() >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
    }
}

#if defined(NEXUS_BACKEND_FREERTOS)
static nx_task_t fixture_task;
NX_OS_STACK_DEFINE(fixture_stack, 1024U);
static volatile nx_result_t fixture_result = NX_ERROR_STATE;

/** \brief Use the same public operations from a statically owned task. */
static void fixture_task_entry(void* context) {
    (void)context;
    fixture_result = exercise();
}

/** \brief Own startup explicitly and preserve the creation result. */
static void bootstrap(void* context) {
    (void)context;
    fixture_result =
        nx_task_start(&fixture_task, "f1-io", fixture_stack,
                      sizeof(fixture_stack), 1U, fixture_task_entry, NULL);
}
#endif

/** \brief Link platform and application lifecycle through public contracts. */
int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
#if defined(NEXUS_BACKEND_FREERTOS)
    (void)nx_os_run(bootstrap, NULL);
    return 2;
#else
    const nx_result_t result = exercise();
    const nx_result_t stopped = nx_platform_stop();
    return result == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
#endif
}

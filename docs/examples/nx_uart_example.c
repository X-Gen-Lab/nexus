/**
 * \file            nx_uart_example.c
 * \brief           Nexus HAL UART Usage Example
 * \author          Nexus Team
 *
 * This example demonstrates how to use the Nexus HAL UART interface:
 * - Getting a UART device using the factory
 * - Configuring UART parameters
 * - Synchronous and asynchronous transmission/reception
 * - Runtime baudrate switching
 * - Device lifecycle management
 *
 * \note            This example is for demonstration purposes and may need
 *                  adaptation for your specific platform and use case.
 */

#include "hal/nx_hal.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Example 1: Basic UART Synchronous Communication                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 1: Basic synchronous UART communication
 * \details         Demonstrates getting a UART device and sending/receiving
 *                  data synchronously with timeout.
 */
void example_uart_sync_basic(void) {
    printf("=== Example 1: Basic UART Synchronous Communication ===\n");

    /* Get UART device using factory */
    nx_uart_t* uart = nx_factory_uart(0);
    if (!uart) {
        printf("Error: Failed to get UART device\n");
        return;
    }

    /* Initialize UART */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize UART\n");
        nx_factory_uart_release(uart);
        return;
    }

    /* Get synchronous TX interface */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    if (!tx_sync) {
        printf("Error: Failed to get TX sync interface\n");
        goto cleanup;
    }

    /* Send data synchronously */
    const char* message = "Hello from Nexus HAL UART!\r\n";
    nx_status_t status =
        tx_sync->send(tx_sync, (const uint8_t*)message, strlen(message), 1000);
    if (status == NX_OK) {
        printf("Successfully sent: %s", message);
    } else {
        printf("Error sending data: %s\n", nx_status_to_string(status));
    }

    /* Get synchronous RX interface */
    nx_rx_sync_t* rx_sync = uart->get_rx_sync(uart);
    if (rx_sync) {
        /* Receive data synchronously */
        uint8_t rx_buffer[64];
        status = rx_sync->receive(rx_sync, rx_buffer, sizeof(rx_buffer), 2000);
        if (status == NX_OK) {
            printf("Received data successfully\n");
        } else {
            printf("Receive timeout or error: %s\n",
                   nx_status_to_string(status));
        }
    }

cleanup:
    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 2: UART Asynchronous Communication with Callbacks                */
/*---------------------------------------------------------------------------*/

/* Callback context */
static volatile bool rx_data_ready = false;

/**
 * \brief           RX callback function
 * \param[in]       context: User context pointer
 */
static void uart_rx_callback(void* context) {
    (void)context;
    rx_data_ready = true;
}

/**
 * \brief           Example 2: Asynchronous UART communication
 * \details         Demonstrates using asynchronous TX/RX with callbacks
 */
void example_uart_async(void) {
    printf("=== Example 2: UART Asynchronous Communication ===\n");

    /* Get and initialize UART */
    nx_uart_t* uart = nx_factory_uart(1);
    if (!uart) {
        printf("Error: Failed to get UART device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize UART\n");
        nx_factory_uart_release(uart);
        return;
    }

    /* Get asynchronous TX interface */
    nx_tx_async_t* tx_async = uart->get_tx_async(uart);
    if (!tx_async) {
        printf("Error: Failed to get TX async interface\n");
        goto cleanup;
    }

    /* Send data asynchronously (non-blocking) */
    const char* message = "Async message\r\n";
    nx_status_t status =
        tx_async->send(tx_async, (const uint8_t*)message, strlen(message));
    if (status == NX_OK) {
        printf("Async send initiated\n");

        /* Check if TX is busy */
        while (tx_async->is_busy(tx_async)) {
            /* Wait for transmission to complete */
        }
        printf("Async send completed\n");
    }

    /* Get asynchronous RX interface and set callback */
    nx_rx_async_t* rx_async = uart->get_rx_async(uart);
    if (rx_async) {
        /* Set RX callback */
        rx_async->set_callback(rx_async, uart_rx_callback, NULL);

        /* Wait for data */
        printf("Waiting for RX data...\n");
        int timeout = 100; /* 100 iterations */
        while (!rx_data_ready && timeout-- > 0) {
            /* Simulate delay */
        }

        if (rx_data_ready) {
            /* Check available data */
            size_t available = rx_async->available(rx_async);
            printf("RX data available: %zu bytes\n", available);

            /* Read data */
            uint8_t rx_buffer[128];
            size_t read_count =
                rx_async->read(rx_async, rx_buffer, sizeof(rx_buffer));
            printf("Read %zu bytes\n", read_count);
        }
    }

cleanup:
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Runtime Configuration and Baudrate Switching                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 3: Runtime configuration
 * \details         Demonstrates runtime baudrate switching and configuration
 *                  management
 */
void example_uart_runtime_config(void) {
    printf("=== Example 3: Runtime Configuration ===\n");

    /* Create custom configuration */
    nx_uart_config_t custom_config = {
        .baudrate = 9600,
        .word_length = 8,
        .stop_bits = 1,
        .parity = 0,       /* No parity */
        .flow_control = 0, /* No flow control */
        .dma_tx_enable = false,
        .dma_rx_enable = false,
        .tx_buf_size = 512,
        .rx_buf_size = 512,
    };

    /* Get UART with custom configuration */
    nx_uart_t* uart = nx_factory_uart_with_config(2, &custom_config);
    if (!uart) {
        printf("Error: Failed to get UART device\n");
        return;
    }

    /* Initialize */
    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize UART\n");
        nx_factory_uart_release(uart);
        return;
    }

    /* Get current configuration */
    nx_uart_config_t current_config;
    if (uart->get_config(uart, &current_config) == NX_OK) {
        printf("Current baudrate: %lu\n",
               (unsigned long)current_config.baudrate);
    }

    /* Switch baudrate at runtime */
    printf("Switching baudrate to 115200...\n");
    if (uart->set_baudrate(uart, 115200) == NX_OK) {
        printf("Baudrate switched successfully\n");

        /* Verify new configuration */
        uart->get_config(uart, &current_config);
        printf("New baudrate: %lu\n", (unsigned long)current_config.baudrate);
    }

    /* Update full configuration */
    custom_config.baudrate = 57600;
    custom_config.stop_bits = 2;
    if (uart->set_config(uart, &custom_config) == NX_OK) {
        printf("Configuration updated successfully\n");
    }

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 4: UART Statistics and Diagnostics                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 4: Statistics and diagnostics
 * \details         Demonstrates how to query UART statistics and status
 */
void example_uart_diagnostics(void) {
    printf("=== Example 4: Statistics and Diagnostics ===\n");

    nx_uart_t* uart = nx_factory_uart(0);
    if (!uart) {
        printf("Error: Failed to get UART device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize UART\n");
        nx_factory_uart_release(uart);
        return;
    }

    /* Send some data to generate statistics */
    nx_tx_sync_t* tx_sync = uart->get_tx_sync(uart);
    if (tx_sync) {
        const char* test_data = "Test data for statistics\r\n";
        tx_sync->send(tx_sync, (const uint8_t*)test_data, strlen(test_data),
                      1000);
    }

    /* Get statistics */
    nx_uart_stats_t stats;
    if (uart->get_stats(uart, &stats) == NX_OK) {
        printf("UART Statistics:\n");
        printf("  TX busy: %s\n", stats.tx_busy ? "Yes" : "No");
        printf("  RX busy: %s\n", stats.rx_busy ? "Yes" : "No");
        printf("  TX count: %lu bytes\n", (unsigned long)stats.tx_count);
        printf("  RX count: %lu bytes\n", (unsigned long)stats.rx_count);
        printf("  TX errors: %lu\n", (unsigned long)stats.tx_errors);
        printf("  RX errors: %lu\n", (unsigned long)stats.rx_errors);
        printf("  Overrun errors: %lu\n", (unsigned long)stats.overrun_errors);
        printf("  Framing errors: %lu\n", (unsigned long)stats.framing_errors);
    }

    /* Get diagnostic interface */
    nx_diagnostic_t* diag = uart->get_diagnostic(uart);
    if (diag) {
        /* Clear statistics */
        diag->clear_statistics(diag);
        printf("Statistics cleared\n");
    }

    /* Clear errors */
    uart->clear_errors(uart);

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 5: Power Management                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 5: Power management
 * \details         Demonstrates suspend/resume and power control
 */
void example_uart_power_management(void) {
    printf("=== Example 5: Power Management ===\n");

    nx_uart_t* uart = nx_factory_uart(0);
    if (!uart) {
        printf("Error: Failed to get UART device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize UART\n");
        nx_factory_uart_release(uart);
        return;
    }

    /* Check device state */
    nx_device_state_t state = lifecycle->get_state(lifecycle);
    printf("Device state: %d (RUNNING=%d)\n", state, NX_DEV_STATE_RUNNING);

    /* Suspend device (low power mode) */
    printf("Suspending UART...\n");
    if (lifecycle->suspend(lifecycle) == NX_OK) {
        printf("UART suspended\n");
        state = lifecycle->get_state(lifecycle);
        printf("Device state: %d (SUSPENDED=%d)\n", state,
               NX_DEV_STATE_SUSPENDED);
    }

    /* Resume device */
    printf("Resuming UART...\n");
    if (lifecycle->resume(lifecycle) == NX_OK) {
        printf("UART resumed\n");
        state = lifecycle->get_state(lifecycle);
        printf("Device state: %d (RUNNING=%d)\n", state, NX_DEV_STATE_RUNNING);
    }

    /* Get power interface */
    nx_power_t* power = uart->get_power(uart);
    if (power) {
        /* Check if power is enabled */
        bool enabled = power->is_enabled(power);
        printf("Power enabled: %s\n", enabled ? "Yes" : "No");

        /* Disable power (clock gating) */
        if (power->disable(power) == NX_OK) {
            printf("Power disabled\n");
        }

        /* Re-enable power */
        if (power->enable(power) == NX_OK) {
            printf("Power enabled\n");
        }
    }

    /* Cleanup */
    lifecycle->deinit(lifecycle);
    nx_factory_uart_release(uart);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Main Function                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main function - runs all UART examples
 */
int main(void) {
    printf("Nexus HAL UART Examples\n");
    printf("=======================\n\n");

    /* Initialize HAL */
    nx_hal_init();

    /* Run examples */
    example_uart_sync_basic();
    example_uart_async();
    example_uart_runtime_config();
    example_uart_diagnostics();
    example_uart_power_management();

    /* Cleanup HAL */
    nx_hal_deinit();

    printf("All examples completed\n");
    return 0;
}


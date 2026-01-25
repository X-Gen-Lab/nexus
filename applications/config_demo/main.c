/**
 * \file            main.c
 * \brief           Config Manager Demo Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-25
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the Config Manager framework
 *                  features:
 *                  - Configuration storage and retrieval
 *                  - Namespace isolation
 *                  - Query and enumeration
 *                  - JSON import/export
 *                  - Binary import/export
 *
 * \note            UART0 is used for output (115200 baud).
 */

#include "framework/config/config.h"
#include "hal/nx_hal.h"
#include "osal/osal.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/

static nx_uart_t* g_uart = NULL; /**< UART device for output */

/*---------------------------------------------------------------------------*/
/* UART Output Functions                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Print string to UART
 */
static void uart_print(const char* str) {
    if (g_uart) {
        nx_tx_sync_t* tx = g_uart->get_tx_sync(g_uart);
        if (tx) {
            tx->send(tx, (const uint8_t*)str, strlen(str), 1000);
        }
    }
}

/**
 * \brief           Print formatted string to UART
 */
static void uart_printf(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    uart_print(buf);
}

/*---------------------------------------------------------------------------*/
/* Demo Functions                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Demonstrate basic configuration storage and retrieval
 */
static void demo_basic_config(void) {
    uart_print("\r\n=== Basic Configuration Demo ===\r\n");

    /* Store different data types */
    config_set_i32("app.timeout", 5000);
    config_set_u32("app.retry", 3);
    config_set_float("sensor.threshold", 25.5f);
    config_set_bool("feature.enabled", true);
    config_set_str("device.name", "Nexus-Demo");

    uart_print("Stored configuration values\r\n");

    /* Read values back */
    int32_t timeout = 0;
    uint32_t retry = 0;
    float threshold = 0.0f;
    bool enabled = false;
    char name[32];

    config_get_i32("app.timeout", &timeout, 0);
    config_get_u32("app.retry", &retry, 0);
    config_get_float("sensor.threshold", &threshold, 0.0f);
    config_get_bool("feature.enabled", &enabled, false);
    config_get_str("device.name", name, sizeof(name));

    uart_printf("  app.timeout = %ld\r\n", (long)timeout);
    uart_printf("  app.retry = %lu\r\n", (unsigned long)retry);
    uart_printf("  sensor.threshold = %.1f\r\n", (double)threshold);
    uart_printf("  feature.enabled = %s\r\n", enabled ? "true" : "false");
    uart_printf("  device.name = %s\r\n", name);
}

/**
 * \brief           Demonstrate namespace isolation
 */
static void demo_namespaces(void) {
    uart_print("\r\n=== Namespace Demo ===\r\n");

    /* Open namespaces */
    config_ns_handle_t wifi_ns, ble_ns;
    config_open_namespace("wifi", &wifi_ns);
    config_open_namespace("ble", &ble_ns);

    /* Store WiFi settings */
    config_ns_set_str(wifi_ns, "ssid", "MyNetwork");
    config_ns_set_bool(wifi_ns, "auto_connect", true);
    config_ns_set_i32(wifi_ns, "channel", 6);

    /* Store BLE settings */
    config_ns_set_str(ble_ns, "name", "Nexus-BLE");
    config_ns_set_u32(ble_ns, "adv_interval", 100);

    uart_print("Stored namespace configurations\r\n");

    /* Read from WiFi namespace */
    char ssid[32];
    bool auto_conn = false;
    int32_t channel = 0;

    config_ns_get_str(wifi_ns, "ssid", ssid, sizeof(ssid));
    config_ns_get_bool(wifi_ns, "auto_connect", &auto_conn, false);
    config_ns_get_i32(wifi_ns, "channel", &channel, 0);

    uart_print("WiFi namespace:\r\n");
    uart_printf("  ssid = %s\r\n", ssid);
    uart_printf("  auto_connect = %s\r\n", auto_conn ? "true" : "false");
    uart_printf("  channel = %ld\r\n", (long)channel);

    /* Read from BLE namespace */
    char ble_name[32];
    uint32_t adv_interval = 0;

    config_ns_get_str(ble_ns, "name", ble_name, sizeof(ble_name));
    config_ns_get_u32(ble_ns, "adv_interval", &adv_interval, 0);

    uart_print("BLE namespace:\r\n");
    uart_printf("  name = %s\r\n", ble_name);
    uart_printf("  adv_interval = %lu\r\n", (unsigned long)adv_interval);

    /* Close namespaces */
    config_close_namespace(wifi_ns);
    config_close_namespace(ble_ns);
}

/**
 * \brief           Iteration callback for listing configs
 * \details         Called for each configuration entry during iteration
 */
static bool list_config_cb(const config_entry_info_t* info, void* user_data) {
    (void)user_data;

    const char* type_str = "unknown";
    switch (info->type) {
        case CONFIG_TYPE_I32:
            type_str = "i32";
            break;
        case CONFIG_TYPE_U32:
            type_str = "u32";
            break;
        case CONFIG_TYPE_I64:
            type_str = "i64";
            break;
        case CONFIG_TYPE_FLOAT:
            type_str = "float";
            break;
        case CONFIG_TYPE_BOOL:
            type_str = "bool";
            break;
        case CONFIG_TYPE_STR:
            type_str = "str";
            break;
        case CONFIG_TYPE_BLOB:
            type_str = "blob";
            break;
        default:
            break;
    }

    uart_printf("  %s [%s, %u bytes]\r\n", info->key, type_str,
                (unsigned)info->value_size);
    return true;
}

/**
 * \brief           Demonstrate query and enumeration
 */
static void demo_query(void) {
    uart_print("\r\n=== Query and Enumeration Demo ===\r\n");

    /* Get total count */
    size_t count = 0;
    config_get_count(&count);
    uart_printf("Total configuration entries: %u\r\n", (unsigned)count);

    /* Check if key exists */
    bool exists = false;
    config_exists("app.timeout", &exists);
    uart_printf("Key 'app.timeout' exists: %s\r\n", exists ? "yes" : "no");

    /* Get value type */
    config_type_t type;
    config_get_type("app.timeout", &type);
    uart_printf("Key 'app.timeout' type: %d (i32=%d)\r\n", type,
                CONFIG_TYPE_I32);

    /* List all entries */
    uart_print("All configuration entries:\r\n");
    config_iterate(list_config_cb, NULL);
}

/**
 * \brief           Demonstrate JSON export
 */
static void demo_json_export(void) {
    uart_print("\r\n=== JSON Export Demo ===\r\n");

    /* Get export size */
    size_t export_size = 0;
    config_get_export_size(CONFIG_FORMAT_JSON, 0, &export_size);
    uart_printf("Required export buffer size: %u bytes\r\n",
                (unsigned)export_size);

    /* Export to JSON */
    char buffer[512];
    size_t actual_size = 0;
    config_status_t status = config_export(CONFIG_FORMAT_JSON, 0, buffer,
                                           sizeof(buffer), &actual_size);

    if (status == CONFIG_OK) {
        uart_printf("Exported %u bytes of JSON\r\n", (unsigned)actual_size);
        /* Print first 100 chars as preview */
        if (actual_size > 100) {
            char preview[101];
            memcpy(preview, buffer, 100);
            preview[100] = '\0';
            uart_printf("Preview: %s...\r\n", preview);
        } else {
            uart_printf("JSON: %s\r\n", buffer);
        }
    } else {
        uart_printf("Export failed: %s\r\n", config_error_to_str(status));
    }
}

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 */
int main(void) {
    /* Initialize OSAL */
    if (osal_init() != OSAL_OK) {
        while (1) {
            /* OSAL initialization failed */
        }
    }

    /* Initialize HAL */
    if (nx_hal_init() != NX_OK) {
        while (1) {
            /* HAL initialization failed */
        }
    }

    /* Get UART device for output */
    g_uart = nx_factory_uart(0);
    if (!g_uart) {
        while (1) {
            /* UART device not available */
        }
    }

    /* Get GPIO devices for status indication */
    nx_gpio_write_t* led0 = nx_factory_gpio_write('A', 0);
    nx_gpio_write_t* led_error = nx_factory_gpio_write('B', 0);

    /* Print welcome message */
    uart_print("\r\n");
    uart_print("========================================\r\n");
    uart_print("  Nexus Config Manager Demo\r\n");
    uart_printf("  HAL Version: %s\r\n", nx_hal_get_version());
    uart_print("========================================\r\n");

    /* Initialize Config Manager */
    config_status_t status = config_init(NULL);
    if (status != CONFIG_OK) {
        uart_printf("Config init failed: %s\r\n", config_error_to_str(status));
        if (led_error) {
            led_error->write(led_error, 1);
        }
        while (1) {
            /* Error state */
        }
    }

    uart_print("Config Manager initialized\r\n");

    /* Turn on LED to indicate ready */
    if (led0) {
        led0->write(led0, 1);
    }

    /* Run demos */
    demo_basic_config();
    demo_namespaces();
    demo_query();
    demo_json_export();

    /* Summary */
    uart_print("\r\n========================================\r\n");
    uart_print("  Demo Complete!\r\n");
    uart_print("========================================\r\n");

    /* Cleanup */
    config_deinit();

    /* Blink LED to indicate success */
    while (1) {
        if (led0) {
            led0->toggle(led0);
        }
        osal_task_delay(500);
    }

    return 0;
}

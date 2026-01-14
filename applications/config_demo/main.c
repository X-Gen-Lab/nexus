/**
 * \file            main.c
 * \brief           Config Manager Demo Application
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This example demonstrates the Config Manager middleware
 *                  on STM32F4 Discovery board. It shows configuration storage,
 *                  retrieval, namespaces, and import/export functionality.
 *
 * \note            UART2 pins: PA2 (TX), PA3 (RX)
 *                  Connect to a serial terminal at 115200 baud.
 */

#include "config/config.h"
#include "hal/hal.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Pin Definitions                                                           */
/*---------------------------------------------------------------------------*/

/** LED pins on STM32F4 Discovery */
#define LED_GREEN_PORT  HAL_GPIO_PORT_D
#define LED_GREEN_PIN   12
#define LED_ORANGE_PORT HAL_GPIO_PORT_D
#define LED_ORANGE_PIN  13
#define LED_RED_PORT    HAL_GPIO_PORT_D
#define LED_RED_PIN     14
#define LED_BLUE_PORT   HAL_GPIO_PORT_D
#define LED_BLUE_PIN    15

/** User button */
#define USER_BTN_PORT HAL_GPIO_PORT_A
#define USER_BTN_PIN  0

/*---------------------------------------------------------------------------*/
/* UART Output                                                               */
/*---------------------------------------------------------------------------*/

static hal_uart_id_t g_uart = HAL_UART_1;

/**
 * \brief           Print string to UART
 */
static void uart_print(const char* str) {
    hal_uart_write(g_uart, (const uint8_t*)str, strlen(str), 1000);
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
                info->value_size);
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
 * \brief           Demonstrate JSON import/export
 */
static void demo_json_export_import(void) {
    uart_print("\r\n=== JSON Export/Import Demo ===\r\n");

    /* Get export size */
    size_t export_size = 0;
    config_get_export_size(CONFIG_FORMAT_JSON, 0, &export_size);
    uart_printf("Required export buffer size: %u bytes\r\n",
                (unsigned)export_size);

    /* Export to JSON */
    char buffer[1024];
    size_t actual_size = 0;
    config_status_t status = config_export(CONFIG_FORMAT_JSON, 0, buffer,
                                           sizeof(buffer), &actual_size);

    if (status == CONFIG_OK) {
        uart_printf("Exported %u bytes of JSON:\r\n", (unsigned)actual_size);
        /* Print JSON (may be truncated for display) */
        if (actual_size < 200) {
            uart_print(buffer);
            uart_print("\r\n");
        } else {
            char preview[200];
            memcpy(preview, buffer, 196);
            preview[196] = '.';
            preview[197] = '.';
            preview[198] = '.';
            preview[199] = '\0';
            uart_print(preview);
            uart_print("\r\n");
        }
    } else {
        uart_printf("Export failed: %s\r\n", config_error_to_str(status));
    }

    /* Demonstrate import by clearing and reimporting */
    uart_print("Clearing configuration...\r\n");

    /* Delete some keys to demonstrate import */
    config_delete("app.timeout");
    config_delete("app.retry");

    /* Verify deletion */
    bool exists = false;
    config_exists("app.timeout", &exists);
    uart_printf("Key 'app.timeout' after delete: %s\r\n",
                exists ? "exists" : "deleted");

    /* Import from JSON */
    uart_print("Importing from JSON...\r\n");
    status = config_import(CONFIG_FORMAT_JSON, 0, buffer, actual_size);

    if (status == CONFIG_OK) {
        uart_print("Import successful\r\n");

        /* Verify import */
        config_exists("app.timeout", &exists);
        uart_printf("Key 'app.timeout' after import: %s\r\n",
                    exists ? "restored" : "missing");

        int32_t timeout = 0;
        config_get_i32("app.timeout", &timeout, 0);
        uart_printf("Value 'app.timeout' = %ld\r\n", (long)timeout);
    } else {
        uart_printf("Import failed: %s\r\n", config_error_to_str(status));
    }
}

/**
 * \brief           Demonstrate binary import/export
 */
static void demo_binary_export_import(void) {
    uart_print("\r\n=== Binary Export/Import Demo ===\r\n");

    /* Store some binary data */
    uint8_t calibration[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    config_set_blob("sensor.cal", calibration, sizeof(calibration));
    uart_print("Stored calibration blob\r\n");

    /* Export to binary */
    uint8_t buffer[512];
    size_t actual_size = 0;
    config_status_t status = config_export(CONFIG_FORMAT_BINARY, 0, buffer,
                                           sizeof(buffer), &actual_size);

    if (status == CONFIG_OK) {
        uart_printf("Exported %u bytes of binary data\r\n",
                    (unsigned)actual_size);
    } else {
        uart_printf("Binary export failed: %s\r\n",
                    config_error_to_str(status));
    }

    /* Delete blob and reimport */
    config_delete("sensor.cal");

    status = config_import(CONFIG_FORMAT_BINARY, 0, buffer, actual_size);
    if (status == CONFIG_OK) {
        uart_print("Binary import successful\r\n");

        /* Verify blob */
        uint8_t read_cal[16];
        size_t read_size = 0;
        config_get_blob("sensor.cal", read_cal, sizeof(read_cal), &read_size);
        uart_printf("Restored calibration blob: %u bytes\r\n",
                    (unsigned)read_size);
    } else {
        uart_printf("Binary import failed: %s\r\n",
                    config_error_to_str(status));
    }
}

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize LEDs
 */
static hal_status_t led_init(void) {
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    if (hal_gpio_init(LED_GREEN_PORT, LED_GREEN_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_ORANGE_PORT, LED_ORANGE_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_RED_PORT, LED_RED_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }
    if (hal_gpio_init(LED_BLUE_PORT, LED_BLUE_PIN, &config) != HAL_OK) {
        return HAL_ERR_FAIL;
    }

    return HAL_OK;
}

/**
 * \brief           Initialize UART for output
 */
static hal_status_t uart_init_output(void) {
    hal_uart_config_t config = {.baudrate = 115200,
                                .wordlen = HAL_UART_WORDLEN_8,
                                .stopbits = HAL_UART_STOPBITS_1,
                                .parity = HAL_UART_PARITY_NONE,
                                .flowctrl = HAL_UART_FLOWCTRL_NONE};

    return hal_uart_init(g_uart, &config);
}

/*---------------------------------------------------------------------------*/
/* Main Entry Point                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main entry point
 */
int main(void) {
    /* Initialize HAL system */
    hal_system_init();

    /* Initialize peripherals */
    if (led_init() != HAL_OK) {
        while (1) {
            hal_gpio_toggle(LED_RED_PORT, LED_RED_PIN);
            hal_delay_ms(100);
        }
    }

    if (uart_init_output() != HAL_OK) {
        while (1) {
            hal_gpio_toggle(LED_ORANGE_PORT, LED_ORANGE_PIN);
            hal_delay_ms(100);
        }
    }

    /* Print welcome message */
    uart_print("\r\n");
    uart_print("========================================\r\n");
    uart_print("  Nexus Config Manager Demo\r\n");
    uart_print("  STM32F4 Discovery Board\r\n");
    uart_print("========================================\r\n");

    /* Initialize Config Manager */
    config_status_t status = config_init(NULL);
    if (status != CONFIG_OK) {
        uart_printf("Config init failed: %s\r\n", config_error_to_str(status));
        while (1) {
            hal_gpio_toggle(LED_BLUE_PORT, LED_BLUE_PIN);
            hal_delay_ms(100);
        }
    }

    uart_print("Config Manager initialized\r\n");

    /* Turn on green LED to indicate ready */
    hal_gpio_write(LED_GREEN_PORT, LED_GREEN_PIN, HAL_GPIO_LEVEL_HIGH);

    /* Run demos */
    demo_basic_config();
    demo_namespaces();
    demo_query();
    demo_json_export_import();
    demo_binary_export_import();

    /* Summary */
    uart_print("\r\n========================================\r\n");
    uart_print("  Demo Complete!\r\n");
    uart_print("========================================\r\n");

    /* Cleanup */
    config_deinit();

    /* Blink green LED to indicate success */
    while (1) {
        hal_gpio_toggle(LED_GREEN_PORT, LED_GREEN_PIN);
        hal_delay_ms(500);
    }

    return 0;
}

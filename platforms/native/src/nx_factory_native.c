/**
 * \file            nx_factory_native.c
 * \brief           Native platform device factory implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations for device get functions */
extern nx_gpio_t* nx_gpio_native_get(uint8_t port, uint8_t pin);
extern nx_gpio_t* nx_gpio_native_get_with_config(uint8_t port, uint8_t pin,
                                                 const nx_gpio_config_t* cfg);
extern nx_uart_t* nx_uart_native_get(uint8_t index);
extern nx_uart_t* nx_uart_native_get_with_config(uint8_t index,
                                                 const nx_uart_config_t* cfg);
extern nx_spi_t* nx_spi_native_get(uint8_t index);
extern nx_spi_t* nx_spi_native_get_with_config(uint8_t index,
                                               const nx_spi_config_t* cfg);
extern nx_i2c_t* nx_i2c_native_get(uint8_t index);
extern nx_i2c_t* nx_i2c_native_get_with_config(uint8_t index,
                                               const nx_i2c_config_t* cfg);
extern nx_timer_t* nx_timer_native_get(uint8_t index);
extern nx_timer_t*
nx_timer_native_get_with_config(uint8_t index, const nx_timer_config_t* cfg);
extern nx_adc_t* nx_adc_native_get(uint8_t index);
extern nx_adc_t* nx_adc_native_get_with_config(uint8_t index,
                                               const nx_adc_config_t* cfg);

/* Forward declarations for device get functions */
extern nx_device_t* nx_gpio_native_get_device(uint8_t port, uint8_t pin);
extern nx_device_t* nx_uart_native_get_device(uint8_t index);
extern nx_device_t* nx_spi_native_get_device(uint8_t index);
extern nx_device_t* nx_i2c_native_get_device(uint8_t index);
extern nx_device_t* nx_timer_native_get_device(uint8_t index);
extern nx_device_t* nx_adc_native_get_device(uint8_t index);

/* ========== GPIO Factory Functions ========== */

nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin) {
    return nx_gpio_native_get(port, pin);
}

nx_gpio_t* nx_factory_gpio_with_config(uint8_t port, uint8_t pin,
                                       const nx_gpio_config_t* cfg) {
    nx_gpio_t* gpio = nx_gpio_native_get(port, pin);
    if (gpio && cfg) {
        /* Initialize device first */
        nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
        if (lifecycle) {
            lifecycle->init(lifecycle);
        }
        /* Apply configuration */
        gpio->set_config(gpio, cfg);
    }
    return gpio;
}

void nx_factory_gpio_release(nx_gpio_t* gpio) {
    /* Native implementation doesn't need explicit release */
    (void)gpio;
}

/* ========== UART Factory Functions ========== */

nx_uart_t* nx_factory_uart(uint8_t index) {
    return nx_uart_native_get(index);
}

nx_uart_t* nx_factory_uart_with_config(uint8_t index,
                                       const nx_uart_config_t* cfg) {
    nx_uart_t* uart = nx_uart_native_get(index);
    if (uart && cfg) {
        /* Initialize device first */
        nx_lifecycle_t* lifecycle = uart->get_lifecycle(uart);
        if (lifecycle) {
            lifecycle->init(lifecycle);
        }
        /* Apply configuration */
        uart->set_config(uart, cfg);
    }
    return uart;
}

void nx_factory_uart_release(nx_uart_t* uart) {
    /* Native implementation doesn't need explicit release */
    (void)uart;
}

/* ========== SPI Factory Functions ========== */

nx_spi_t* nx_factory_spi(uint8_t index) {
    return nx_spi_native_get(index);
}

nx_spi_t* nx_factory_spi_with_config(uint8_t index,
                                     const nx_spi_config_t* cfg) {
    nx_spi_t* spi = nx_spi_native_get(index);
    if (spi && cfg) {
        /* Apply configuration */
        spi->set_config(spi, cfg);
    }
    return spi;
}

void nx_factory_spi_release(nx_spi_t* spi) {
    /* Native implementation doesn't need explicit release */
    (void)spi;
}

/* ========== I2C Factory Functions ========== */

nx_i2c_t* nx_factory_i2c(uint8_t index) {
    return nx_i2c_native_get(index);
}

nx_i2c_t* nx_factory_i2c_with_config(uint8_t index,
                                     const nx_i2c_config_t* cfg) {
    nx_i2c_t* i2c = nx_i2c_native_get(index);
    if (i2c && cfg) {
        /* Apply configuration */
        i2c->set_config(i2c, cfg);
    }
    return i2c;
}

void nx_factory_i2c_release(nx_i2c_t* i2c) {
    /* Native implementation doesn't need explicit release */
    (void)i2c;
}

/* ========== Timer Factory Functions ========== */

nx_timer_t* nx_factory_timer(uint8_t index) {
    return nx_timer_native_get(index);
}

nx_timer_t* nx_factory_timer_with_config(uint8_t index,
                                         const nx_timer_config_t* cfg) {
    if (cfg == NULL) {
        return nx_timer_native_get(index);
    }
    return nx_timer_native_get_with_config(index, cfg);
}

void nx_factory_timer_release(nx_timer_t* timer) {
    /* Native implementation doesn't need explicit release */
    (void)timer;
}

/* ========== ADC Factory Functions ========== */

nx_adc_t* nx_factory_adc(uint8_t index) {
    return nx_adc_native_get(index);
}

nx_adc_t* nx_factory_adc_with_config(uint8_t index,
                                     const nx_adc_config_t* cfg) {
    if (cfg == NULL) {
        return nx_adc_native_get(index);
    }
    return nx_adc_native_get_with_config(index, cfg);
}

void nx_factory_adc_release(nx_adc_t* adc) {
    /* Native implementation doesn't need explicit release */
    (void)adc;
}

/* ========== Device Enumeration ========== */

/**
 * \brief           Maximum devices per type
 */
#define NX_GPIO_MAX_DEVICES  128 /* 8 ports * 16 pins */
#define NX_UART_MAX_DEVICES  6
#define NX_SPI_MAX_DEVICES   3
#define NX_I2C_MAX_DEVICES   3
#define NX_TIMER_MAX_DEVICES 14
#define NX_ADC_MAX_DEVICES   3

size_t nx_factory_enumerate(nx_device_info_t* list, size_t max_count) {
    if (list == NULL || max_count == 0) {
        return 0;
    }

    size_t count = 0;

    /* Static storage for device names */
    static char device_names[256][16];
    size_t name_index = 0;

    /* Enumerate GPIO devices (port 0-7, pin 0-15) */
    for (uint8_t port = 0; port < 8 && count < max_count; port++) {
        for (uint8_t pin = 0; pin < 16 && count < max_count && name_index < 256;
             pin++) {
            /* Create device name */
            snprintf(device_names[name_index], 16, "gpio%c%d", 'a' + port, pin);

            list[count].name = device_names[name_index];
            list[count].type = "gpio";
            list[count].state = NX_DEV_STATE_UNINITIALIZED;
            list[count].ref_count = 0;
            count++;
            name_index++;
        }
    }

    /* Enumerate UART devices */
    for (uint8_t i = 0;
         i < NX_UART_MAX_DEVICES && count < max_count && name_index < 256;
         i++) {
        snprintf(device_names[name_index], 16, "uart%d", i);

        list[count].name = device_names[name_index];
        list[count].type = "uart";
        list[count].state = NX_DEV_STATE_UNINITIALIZED;
        list[count].ref_count = 0;
        count++;
        name_index++;
    }

    /* Enumerate SPI devices */
    for (uint8_t i = 0;
         i < NX_SPI_MAX_DEVICES && count < max_count && name_index < 256; i++) {
        snprintf(device_names[name_index], 16, "spi%d", i);

        list[count].name = device_names[name_index];
        list[count].type = "spi";
        list[count].state = NX_DEV_STATE_UNINITIALIZED;
        list[count].ref_count = 0;
        count++;
        name_index++;
    }

    /* Enumerate I2C devices */
    for (uint8_t i = 0;
         i < NX_I2C_MAX_DEVICES && count < max_count && name_index < 256; i++) {
        snprintf(device_names[name_index], 16, "i2c%d", i);

        list[count].name = device_names[name_index];
        list[count].type = "i2c";
        list[count].state = NX_DEV_STATE_UNINITIALIZED;
        list[count].ref_count = 0;
        count++;
        name_index++;
    }

    /* Enumerate Timer devices */
    for (uint8_t i = 0;
         i < NX_TIMER_MAX_DEVICES && count < max_count && name_index < 256;
         i++) {
        snprintf(device_names[name_index], 16, "timer%d", i);

        list[count].name = device_names[name_index];
        list[count].type = "timer";
        list[count].state = NX_DEV_STATE_UNINITIALIZED;
        list[count].ref_count = 0;
        count++;
        name_index++;
    }

    /* Enumerate ADC devices */
    for (uint8_t i = 0;
         i < NX_ADC_MAX_DEVICES && count < max_count && name_index < 256; i++) {
        snprintf(device_names[name_index], 16, "adc%d", i);

        list[count].name = device_names[name_index];
        list[count].type = "adc";
        list[count].state = NX_DEV_STATE_UNINITIALIZED;
        list[count].ref_count = 0;
        count++;
        name_index++;
    }

    return count;
}

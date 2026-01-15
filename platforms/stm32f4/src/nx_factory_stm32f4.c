/**
 * \file            nx_factory_stm32f4.c
 * \brief           STM32F4 platform device factory implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_device.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <string.h>

/* Forward declarations for device get functions */
extern nx_device_t* nx_gpio_stm32f4_get_device(uint8_t port, uint8_t pin);
extern nx_device_t* nx_uart_stm32f4_get_device(uint8_t index);
extern nx_device_t* nx_spi_stm32f4_get_device(uint8_t index);
extern nx_device_t* nx_i2c_stm32f4_get_device(uint8_t index);
extern nx_device_t* nx_timer_stm32f4_get_device(uint8_t index);
extern nx_device_t* nx_adc_stm32f4_get_device(uint8_t index);

/* ========== GPIO Factory Functions ========== */

nx_gpio_t* nx_factory_gpio(uint8_t port, uint8_t pin) {
    return nx_factory_gpio_with_config(port, pin, NULL);
}

nx_gpio_t* nx_factory_gpio_with_config(uint8_t port, uint8_t pin,
                                       const nx_gpio_config_t* cfg) {
    nx_device_t* dev = nx_gpio_stm32f4_get_device(port, pin);
    if (dev == NULL) {
        return NULL;
    }

    /* If configuration provided, reinitialize with new config */
    if (cfg != NULL) {
        nx_status_t status = nx_device_reinit(dev, cfg);
        if (status != NX_OK) {
            return NULL;
        }
    }

    /* Get device interface using reference counting */
    return (nx_gpio_t*)nx_device_get(dev->name);
}

void nx_factory_gpio_release(nx_gpio_t* gpio) {
    if (gpio != NULL) {
        nx_device_put(gpio);
    }
}

/* ========== UART Factory Functions ========== */

nx_uart_t* nx_factory_uart(uint8_t index) {
    return nx_factory_uart_with_config(index, NULL);
}

nx_uart_t* nx_factory_uart_with_config(uint8_t index,
                                       const nx_uart_config_t* cfg) {
    nx_device_t* dev = nx_uart_stm32f4_get_device(index);
    if (dev == NULL) {
        return NULL;
    }

    /* If configuration provided, reinitialize with new config */
    if (cfg != NULL) {
        nx_status_t status = nx_device_reinit(dev, cfg);
        if (status != NX_OK) {
            return NULL;
        }
    }

    /* Get device interface using reference counting */
    return (nx_uart_t*)nx_device_get(dev->name);
}

void nx_factory_uart_release(nx_uart_t* uart) {
    if (uart != NULL) {
        nx_device_put(uart);
    }
}

/* ========== SPI Factory Functions ========== */

nx_spi_t* nx_factory_spi(uint8_t index) {
    return nx_factory_spi_with_config(index, NULL);
}

nx_spi_t* nx_factory_spi_with_config(uint8_t index,
                                     const nx_spi_config_t* cfg) {
    nx_device_t* dev = nx_spi_stm32f4_get_device(index);
    if (dev == NULL) {
        return NULL;
    }

    /* If configuration provided, reinitialize with new config */
    if (cfg != NULL) {
        nx_status_t status = nx_device_reinit(dev, cfg);
        if (status != NX_OK) {
            return NULL;
        }
    }

    /* Get device interface using reference counting */
    return (nx_spi_t*)nx_device_get(dev->name);
}

void nx_factory_spi_release(nx_spi_t* spi) {
    if (spi != NULL) {
        nx_device_put(spi);
    }
}

/* ========== I2C Factory Functions ========== */

nx_i2c_t* nx_factory_i2c(uint8_t index) {
    return nx_factory_i2c_with_config(index, NULL);
}

nx_i2c_t* nx_factory_i2c_with_config(uint8_t index,
                                     const nx_i2c_config_t* cfg) {
    nx_device_t* dev = nx_i2c_stm32f4_get_device(index);
    if (dev == NULL) {
        return NULL;
    }

    /* If configuration provided, reinitialize with new config */
    if (cfg != NULL) {
        nx_status_t status = nx_device_reinit(dev, cfg);
        if (status != NX_OK) {
            return NULL;
        }
    }

    /* Get device interface using reference counting */
    return (nx_i2c_t*)nx_device_get(dev->name);
}

void nx_factory_i2c_release(nx_i2c_t* i2c) {
    if (i2c != NULL) {
        nx_device_put(i2c);
    }
}

/* ========== Timer Factory Functions ========== */

nx_timer_t* nx_factory_timer(uint8_t index) {
    return nx_factory_timer_with_config(index, NULL);
}

nx_timer_t* nx_factory_timer_with_config(uint8_t index,
                                         const nx_timer_config_t* cfg) {
    nx_device_t* dev = nx_timer_stm32f4_get_device(index);
    if (dev == NULL) {
        return NULL;
    }

    /* If configuration provided, reinitialize with new config */
    if (cfg != NULL) {
        nx_status_t status = nx_device_reinit(dev, cfg);
        if (status != NX_OK) {
            return NULL;
        }
    }

    /* Get device interface using reference counting */
    return (nx_timer_t*)nx_device_get(dev->name);
}

void nx_factory_timer_release(nx_timer_t* timer) {
    if (timer != NULL) {
        nx_device_put(timer);
    }
}

/* ========== ADC Factory Functions ========== */

nx_adc_t* nx_factory_adc(uint8_t index) {
    return nx_factory_adc_with_config(index, NULL);
}

nx_adc_t* nx_factory_adc_with_config(uint8_t index,
                                     const nx_adc_config_t* cfg) {
    nx_device_t* dev = nx_adc_stm32f4_get_device(index);
    if (dev == NULL) {
        return NULL;
    }

    /* If configuration provided, reinitialize with new config */
    if (cfg != NULL) {
        nx_status_t status = nx_device_reinit(dev, cfg);
        if (status != NX_OK) {
            return NULL;
        }
    }

    /* Get device interface using reference counting */
    return (nx_adc_t*)nx_device_get(dev->name);
}

void nx_factory_adc_release(nx_adc_t* adc) {
    if (adc != NULL) {
        nx_device_put(adc);
    }
}

/* ========== Device Enumeration ========== */

/**
 * \brief           Device type strings
 */
static const char* device_type_strings[] = {
    "gpio", "uart", "spi", "i2c", "timer", "adc",
};

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

    /* Enumerate GPIO devices (port 0-7, pin 0-15) */
    for (uint8_t port = 0; port < 8 && count < max_count; port++) {
        for (uint8_t pin = 0; pin < 16 && count < max_count; pin++) {
            nx_device_t* dev = nx_gpio_stm32f4_get_device(port, pin);
            if (dev != NULL) {
                list[count].name = dev->name;
                list[count].type = "gpio";
                list[count].state = dev->state.state;
                list[count].ref_count = dev->state.ref_count;
                count++;
            }
        }
    }

    /* Enumerate UART devices */
    for (uint8_t i = 0; i < NX_UART_MAX_DEVICES && count < max_count; i++) {
        nx_device_t* dev = nx_uart_stm32f4_get_device(i);
        if (dev != NULL) {
            list[count].name = dev->name;
            list[count].type = "uart";
            list[count].state = dev->state.state;
            list[count].ref_count = dev->state.ref_count;
            count++;
        }
    }

    /* Enumerate SPI devices */
    for (uint8_t i = 0; i < NX_SPI_MAX_DEVICES && count < max_count; i++) {
        nx_device_t* dev = nx_spi_stm32f4_get_device(i);
        if (dev != NULL) {
            list[count].name = dev->name;
            list[count].type = "spi";
            list[count].state = dev->state.state;
            list[count].ref_count = dev->state.ref_count;
            count++;
        }
    }

    /* Enumerate I2C devices */
    for (uint8_t i = 0; i < NX_I2C_MAX_DEVICES && count < max_count; i++) {
        nx_device_t* dev = nx_i2c_stm32f4_get_device(i);
        if (dev != NULL) {
            list[count].name = dev->name;
            list[count].type = "i2c";
            list[count].state = dev->state.state;
            list[count].ref_count = dev->state.ref_count;
            count++;
        }
    }

    /* Enumerate Timer devices */
    for (uint8_t i = 0; i < NX_TIMER_MAX_DEVICES && count < max_count; i++) {
        nx_device_t* dev = nx_timer_stm32f4_get_device(i);
        if (dev != NULL) {
            list[count].name = dev->name;
            list[count].type = "timer";
            list[count].state = dev->state.state;
            list[count].ref_count = dev->state.ref_count;
            count++;
        }
    }

    /* Enumerate ADC devices */
    for (uint8_t i = 0; i < NX_ADC_MAX_DEVICES && count < max_count; i++) {
        nx_device_t* dev = nx_adc_stm32f4_get_device(i);
        if (dev != NULL) {
            list[count].name = dev->name;
            list[count].type = "adc";
            list[count].state = dev->state.state;
            list[count].ref_count = dev->state.ref_count;
            count++;
        }
    }

    return count;
}

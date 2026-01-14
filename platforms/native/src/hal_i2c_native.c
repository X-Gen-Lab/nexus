/**
 * \file            hal_i2c_native.c
 * \brief           Native I2C Implementation (Simulation)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation simulates I2C communication for testing purposes
 * on the native platform. It provides device simulation and tracks
 * protocol compliance for verification.
 */

#include "hal/hal_i2c.h"
#include "native_platform.h"
#include <string.h>

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

#define I2C_BUFFER_SIZE 256
#define I2C_MAX_DEVICES 16

/**
 * \brief           Simulated I2C device
 */
typedef struct {
    uint16_t address;                /**< Device address */
    bool ready;                      /**< Device ready flag */
    uint8_t memory[I2C_BUFFER_SIZE]; /**< Device memory */
} i2c_device_t;

/**
 * \brief           I2C instance state
 */
typedef struct {
    bool initialized;
    hal_i2c_config_t config;
    hal_i2c_callback_t callback;
    void* context;
    uint32_t actual_speed_hz;              /**< Actual configured speed */
    i2c_device_t devices[I2C_MAX_DEVICES]; /**< Simulated devices */
    uint8_t device_count;                  /**< Number of registered devices */
    uint8_t last_tx_buffer[I2C_BUFFER_SIZE]; /**< Last transmitted data */
    uint8_t last_rx_buffer[I2C_BUFFER_SIZE]; /**< Last received data */
    size_t last_tx_len;                      /**< Length of last TX */
    size_t last_rx_len;                      /**< Length of last RX */
    uint16_t last_dev_addr;                  /**< Last device address used */
    uint16_t last_mem_addr;                  /**< Last memory address used */
    uint8_t last_mem_addr_size;              /**< Last memory address size */
} i2c_state_t;

static i2c_state_t i2c_instances[HAL_I2C_MAX];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Find device by address
 */
static i2c_device_t* find_device(i2c_state_t* i2c, uint16_t dev_addr) {
    for (uint8_t i = 0; i < i2c->device_count; i++) {
        if (i2c->devices[i].address == dev_addr) {
            return &i2c->devices[i];
        }
    }
    return NULL;
}

/**
 * \brief           Get speed in Hz from speed enum
 */
static uint32_t get_speed_hz(hal_i2c_speed_t speed) {
    switch (speed) {
        case HAL_I2C_SPEED_STANDARD:
            return 100000; /* 100 kHz */
        case HAL_I2C_SPEED_FAST:
            return 400000; /* 400 kHz */
        case HAL_I2C_SPEED_FAST_PLUS:
            return 1000000; /* 1 MHz */
        default:
            return 100000;
    }
}

/*===========================================================================*/
/* Public functions - Test helpers                                            */
/*===========================================================================*/

void native_i2c_reset_all(void) {
    memset(i2c_instances, 0, sizeof(i2c_instances));
}

native_i2c_state_t* native_i2c_get_state(int instance) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return NULL;
    }
    return (native_i2c_state_t*)&i2c_instances[instance];
}

bool native_i2c_is_initialized(int instance) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return false;
    }
    return i2c_instances[instance].initialized;
}

uint32_t native_i2c_get_actual_speed(int instance) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return 0;
    }
    return i2c_instances[instance].actual_speed_hz;
}

bool native_i2c_add_device(int instance, uint16_t dev_addr, bool ready) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return false;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    if (i2c->device_count >= I2C_MAX_DEVICES) {
        return false;
    }

    /* Check if device already exists */
    if (find_device(i2c, dev_addr) != NULL) {
        return false;
    }

    i2c_device_t* device = &i2c->devices[i2c->device_count];
    device->address = dev_addr;
    device->ready = ready;
    memset(device->memory, 0, I2C_BUFFER_SIZE);
    i2c->device_count++;

    return true;
}

bool native_i2c_set_device_ready(int instance, uint16_t dev_addr, bool ready) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return false;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL) {
        return false;
    }

    device->ready = ready;
    return true;
}

bool native_i2c_write_device_memory(int instance, uint16_t dev_addr,
                                    uint16_t mem_addr, const uint8_t* data,
                                    size_t len) {
    if (instance < 0 || instance >= HAL_I2C_MAX || data == NULL) {
        return false;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL || !device->ready) {
        return false;
    }

    if (mem_addr + len > I2C_BUFFER_SIZE) {
        return false;
    }

    memcpy(&device->memory[mem_addr], data, len);
    return true;
}

bool native_i2c_read_device_memory(int instance, uint16_t dev_addr,
                                   uint16_t mem_addr, uint8_t* data,
                                   size_t len) {
    if (instance < 0 || instance >= HAL_I2C_MAX || data == NULL) {
        return false;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL || !device->ready) {
        return false;
    }

    if (mem_addr + len > I2C_BUFFER_SIZE) {
        return false;
    }

    memcpy(data, &device->memory[mem_addr], len);
    return true;
}

size_t native_i2c_get_last_tx_data(int instance, uint8_t* data,
                                   size_t max_len) {
    if (instance < 0 || instance >= HAL_I2C_MAX || data == NULL) {
        return 0;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    size_t len = i2c->last_tx_len;
    if (len > max_len) {
        len = max_len;
    }
    memcpy(data, i2c->last_tx_buffer, len);
    return len;
}

size_t native_i2c_get_last_rx_data(int instance, uint8_t* data,
                                   size_t max_len) {
    if (instance < 0 || instance >= HAL_I2C_MAX || data == NULL) {
        return 0;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    size_t len = i2c->last_rx_len;
    if (len > max_len) {
        len = max_len;
    }
    memcpy(data, i2c->last_rx_buffer, len);
    return len;
}

uint16_t native_i2c_get_last_dev_addr(int instance) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return 0;
    }
    return i2c_instances[instance].last_dev_addr;
}

uint16_t native_i2c_get_last_mem_addr(int instance) {
    if (instance < 0 || instance >= HAL_I2C_MAX) {
        return 0;
    }
    return i2c_instances[instance].last_mem_addr;
}

/*===========================================================================*/
/* Public functions - HAL API                                                 */
/*===========================================================================*/

hal_status_t hal_i2c_init(hal_i2c_instance_t instance,
                          const hal_i2c_config_t* config) {
    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }

    /* Validate speed mode */
    if (config->speed > HAL_I2C_SPEED_FAST_PLUS) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Validate address mode */
    if (config->addr_mode > HAL_I2C_ADDR_10BIT) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    memcpy(&i2c->config, config, sizeof(hal_i2c_config_t));
    i2c->initialized = true;
    i2c->callback = NULL;
    i2c->context = NULL;
    i2c->actual_speed_hz = get_speed_hz(config->speed);
    i2c->device_count = 0;
    i2c->last_tx_len = 0;
    i2c->last_rx_len = 0;
    i2c->last_dev_addr = 0;
    i2c->last_mem_addr = 0;
    i2c->last_mem_addr_size = 0;
    memset(i2c->devices, 0, sizeof(i2c->devices));
    memset(i2c->last_tx_buffer, 0, I2C_BUFFER_SIZE);
    memset(i2c->last_rx_buffer, 0, I2C_BUFFER_SIZE);

    return HAL_OK;
}

hal_status_t hal_i2c_deinit(hal_i2c_instance_t instance) {
    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    i2c->initialized = false;
    i2c->callback = NULL;
    i2c->context = NULL;
    i2c->device_count = 0;
    i2c->last_tx_len = 0;
    i2c->last_rx_len = 0;

    return HAL_OK;
}

hal_status_t hal_i2c_master_transmit(hal_i2c_instance_t instance,
                                     uint16_t dev_addr, const uint8_t* data,
                                     size_t len, uint32_t timeout_ms) {
    (void)timeout_ms;

    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (len == 0 || len > I2C_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    if (!i2c->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Find device */
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL) {
        return HAL_ERROR_TIMEOUT; /* Device not found = no ACK */
    }
    if (!device->ready) {
        return HAL_ERROR_TIMEOUT; /* Device not ready = no ACK */
    }

    /* Store transaction details for testing */
    i2c->last_dev_addr = dev_addr;
    memcpy(i2c->last_tx_buffer, data, len);
    i2c->last_tx_len = len;

    /* Invoke callback if registered */
    if (i2c->callback != NULL) {
        i2c->callback(instance, 0, i2c->context);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_master_receive(hal_i2c_instance_t instance,
                                    uint16_t dev_addr, uint8_t* data,
                                    size_t len, uint32_t timeout_ms) {
    (void)timeout_ms;

    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (len == 0 || len > I2C_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    if (!i2c->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Find device */
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL) {
        return HAL_ERROR_TIMEOUT; /* Device not found = no ACK */
    }
    if (!device->ready) {
        return HAL_ERROR_TIMEOUT; /* Device not ready = no ACK */
    }

    /* Return data from device memory (starting from address 0) */
    memcpy(data, device->memory, len);

    /* Store transaction details for testing */
    i2c->last_dev_addr = dev_addr;
    memcpy(i2c->last_rx_buffer, data, len);
    i2c->last_rx_len = len;

    /* Invoke callback if registered */
    if (i2c->callback != NULL) {
        i2c->callback(instance, 0, i2c->context);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_mem_write(hal_i2c_instance_t instance, uint16_t dev_addr,
                               uint16_t mem_addr, uint8_t mem_addr_size,
                               const uint8_t* data, size_t len,
                               uint32_t timeout_ms) {
    (void)timeout_ms;

    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (len == 0 || len > I2C_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (mem_addr_size != 1 && mem_addr_size != 2) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    if (!i2c->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Find device */
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL) {
        return HAL_ERROR_TIMEOUT; /* Device not found = no ACK */
    }
    if (!device->ready) {
        return HAL_ERROR_TIMEOUT; /* Device not ready = no ACK */
    }

    /* Check memory bounds */
    if (mem_addr + len > I2C_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Write to device memory */
    memcpy(&device->memory[mem_addr], data, len);

    /* Store transaction details for testing */
    i2c->last_dev_addr = dev_addr;
    i2c->last_mem_addr = mem_addr;
    i2c->last_mem_addr_size = mem_addr_size;
    memcpy(i2c->last_tx_buffer, data, len);
    i2c->last_tx_len = len;

    /* Invoke callback if registered */
    if (i2c->callback != NULL) {
        i2c->callback(instance, 0, i2c->context);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_mem_read(hal_i2c_instance_t instance, uint16_t dev_addr,
                              uint16_t mem_addr, uint8_t mem_addr_size,
                              uint8_t* data, size_t len, uint32_t timeout_ms) {
    (void)timeout_ms;

    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (len == 0 || len > I2C_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (mem_addr_size != 1 && mem_addr_size != 2) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    if (!i2c->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Find device */
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL) {
        return HAL_ERROR_TIMEOUT; /* Device not found = no ACK */
    }
    if (!device->ready) {
        return HAL_ERROR_TIMEOUT; /* Device not ready = no ACK */
    }

    /* Check memory bounds */
    if (mem_addr + len > I2C_BUFFER_SIZE) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Read from device memory */
    memcpy(data, &device->memory[mem_addr], len);

    /* Store transaction details for testing */
    i2c->last_dev_addr = dev_addr;
    i2c->last_mem_addr = mem_addr;
    i2c->last_mem_addr_size = mem_addr_size;
    memcpy(i2c->last_rx_buffer, data, len);
    i2c->last_rx_len = len;

    /* Invoke callback if registered */
    if (i2c->callback != NULL) {
        i2c->callback(instance, 0, i2c->context);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_is_device_ready(hal_i2c_instance_t instance,
                                     uint16_t dev_addr, uint8_t retries,
                                     uint32_t timeout_ms) {
    (void)retries;
    (void)timeout_ms;

    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    if (!i2c->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Find device */
    i2c_device_t* device = find_device(i2c, dev_addr);
    if (device == NULL) {
        return HAL_ERROR_TIMEOUT; /* Device not found */
    }

    return device->ready ? HAL_OK : HAL_ERROR_TIMEOUT;
}

hal_status_t hal_i2c_set_callback(hal_i2c_instance_t instance,
                                  hal_i2c_callback_t callback, void* context) {
    if (instance >= HAL_I2C_MAX) {
        return HAL_ERROR_INVALID_PARAM;
    }

    i2c_state_t* i2c = &i2c_instances[instance];
    i2c->callback = callback;
    i2c->context = context;

    return HAL_OK;
}
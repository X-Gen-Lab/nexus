/** Native I2C model: immutable device identities and bounded resources. */
#ifndef NX_I2C_TYPES_H
#define NX_I2C_TYPES_H
#include "hal/interface/nx_i2c.h"
#include "osal/osal_mutex.h"

#define NATIVE_I2C_DEVICE_CAPACITY 16u
#define NATIVE_I2C_LEGACY_CAPACITY 256u
#define NATIVE_I2C_RESPONSE_CAPACITY 16u
#define NATIVE_I2C_PAYLOAD_CAPACITY 256u

typedef struct nx_device_s nx_device_t;
typedef struct nx_i2c_impl_s nx_i2c_impl_t;
typedef struct nx_i2c_platform_config_s {
    uint8_t i2c_index;
    uint32_t speed;
    uint8_t scl_pin, sda_pin;
    size_t tx_buf_size, rx_buf_size;
} nx_i2c_platform_config_t;
typedef struct nx_i2c_buffer_s {
    uint8_t* data;
    size_t size, head, tail, count;
} nx_i2c_buffer_t;
typedef struct nx_i2c_config_s {
    uint32_t speed;
    uint8_t scl_pin, sda_pin;
    bool dma_tx_enable, dma_rx_enable;
    size_t tx_buf_size, rx_buf_size;
} nx_i2c_config_t;
typedef struct nx_i2c_device_handle_s {
    uint8_t dev_addr;
    nx_comm_callback_t callback;
    void* user_data;
    bool in_use;
} nx_i2c_device_handle_t;
typedef struct nx_i2c_stats_s {
    uint32_t tx_count, rx_count, nack_count, bus_error_count;
} nx_i2c_stats_t;
typedef struct nx_i2c_state_s {
    nx_i2c_stats_t stats;
    uint8_t index;
    nx_i2c_config_t config;
    nx_i2c_buffer_t tx_buf, rx_buf; /* RX buffer is the explicit wildcard fixture. */
    nx_i2c_device_handle_t current_device; /* Last executed diagnostic snapshot. */
    uint8_t current_dev_addr;
    bool initialized, suspended, busy;
} nx_i2c_state_t;
typedef struct native_i2c_device_s {
    nx_i2c_device_t base;
    nx_i2c_impl_t* bus;
    uint8_t address;
    bool allocated, legacy, cancelled, completing;
    unsigned users;
    nx_comm_callback_t receive_callback;
    void* receive_context;
    nx_status_t last_result;
    nx_tx_sync_t tx_sync;
    nx_tx_rx_sync_t tx_rx_sync;
    nx_tx_async_t tx_async;
    nx_tx_rx_async_t tx_rx_async;
} native_i2c_device_t;
typedef struct native_i2c_response_s {
    bool used;
    uint8_t address;
    uint8_t data[NATIVE_I2C_PAYLOAD_CAPACITY];
    size_t length, offset;
} native_i2c_response_t;
struct nx_i2c_impl_s {
    nx_i2c_bus_t base;
    nx_lifecycle_t lifecycle;
    nx_power_t power;
    nx_i2c_state_t* state;
    nx_device_t* device;
    osal_mutex_handle_t mutex;
    unsigned users;
    uint64_t next_token;
    native_i2c_device_t devices[NATIVE_I2C_DEVICE_CAPACITY];
    native_i2c_device_t legacy_devices[NATIVE_I2C_LEGACY_CAPACITY];
    native_i2c_response_t responses[NATIVE_I2C_RESPONSE_CAPACITY];
    native_i2c_device_t* pending;
    native_i2c_device_t* active;
    nx_i2c_transaction_t queued;
    uint64_t queued_at;
    bool worker_active;
    uint8_t tx_copy[NATIVE_I2C_PAYLOAD_CAPACITY];
    uint8_t rx_copy[NATIVE_I2C_PAYLOAD_CAPACITY];
    size_t received_length;
    uint32_t transfer_delay_ms;
    nx_status_t next_failure;
    nx_power_callback_t power_callback;
    void* power_context;
};
#endif

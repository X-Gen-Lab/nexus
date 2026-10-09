/* Exercise production startup without the HAL test registration helpers. */
#include "hal/nx_factory.h"
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "hal/nx_hal.h"
#include "nexus_board.h"
#include "osal/osal.h"
#include <assert.h>
#include <stdio.h>

static void* conflicting_init(const nx_device_t* device) {
    (void)device;
    return NULL;
}

int main(void) {
    assert(!nx_hal_is_initialized());
    nx_device_config_state_t conflict_state = {0};
    const nx_device_t conflict = {
        .name = "UART0", .state = &conflict_state,
        .device_init = conflicting_init};
    assert(nx_device_register(&conflict) == NX_OK);
    assert(nx_hal_init() == NX_ERR_ALREADY_INIT);
    assert(!nx_hal_is_initialized());
    nx_device_clear_all();

    assert(nx_hal_init() == NX_OK);
    assert(nx_hal_init() == NX_OK);
    assert(osal_init() == OSAL_OK);
    const nx_device_t* uart = nx_device_find("UART0");
    assert(uart && uart != &conflict);
    assert(nx_factory_uart(0));
    assert(nx_factory_spi(0));
    assert(nx_factory_i2c(0));
    nx_gpio_write_t* gpio = nx_factory_gpio_write(
        NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN);
    nx_gpio_read_t* input = nx_factory_gpio_read(
        NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN);
    assert(gpio && input);
    nx_lifecycle_t* lifecycle = gpio->get_lifecycle(gpio);
    assert(lifecycle && lifecycle->init(lifecycle) == NX_OK);
    gpio->write(gpio, 1);
    assert(input->read(input) == 1);
    assert(nx_hal_deinit() == NX_ERR_BUSY);
    assert(osal_deinit() == OSAL_OK);
    assert(nx_hal_deinit() == NX_OK);
    assert(!nx_hal_is_initialized());

    assert(nx_hal_init() == NX_OK);
    assert(nx_device_find("UART0") == uart);
    assert(nx_factory_gpio_write(
        NX_BOARD_LED_GPIO_PORT, NX_BOARD_LED_GPIO_PIN) == gpio);
    assert(lifecycle->init(lifecycle) == NX_OK);
    assert(input->read(input) == 0);
    assert(nx_hal_deinit() == NX_OK);
    puts("Production Native registry: startup failure, retry and lifecycle passed");
    return 0;
}

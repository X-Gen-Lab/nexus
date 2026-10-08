/** Real registered STM32 GPIO classes/storage with a test-only electrical port. */
#include "hal/base/nx_device.h"
#include "hal/nx_factory.h"
#include "stm32_gpio.h"
#include <assert.h>
#include <stdio.h>
GPIO_TypeDef fake_gpio_a, fake_gpio_d;
static unsigned clocks, initialized;
uint32_t __get_IPSR(void) { return 0; }
void fake_gpio_clock(GPIO_TypeDef* port) { assert(port == GPIOD); ++clocks; }
void HAL_GPIO_Init(GPIO_TypeDef* port, GPIO_InitTypeDef* gpio) {
    assert(port == GPIOD && clocks);
    assert(gpio->Pin == GPIO_PIN_12 || gpio->Pin == GPIO_PIN_13 || gpio->Pin == GPIO_PIN_14);
    ++initialized;
}
void HAL_GPIO_DeInit(GPIO_TypeDef* port, uint32_t pin) { assert(port == GPIOD && pin); --initialized; }
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState state) {
    if (state) port->value |= pin; else port->value &= (uint16_t)~pin;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin) { return port->value & pin ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin) { port->value ^= pin; }
int main(void) {
    extern const nx_device_t STM32_GPIOD12, STM32_GPIOD13, STM32_GPIOD14;
    assert(nx_device_register(&STM32_GPIOD12) == NX_OK);
    assert(nx_device_register(&STM32_GPIOD13) == NX_OK);
    assert(nx_device_register(&STM32_GPIOD14) == NX_OK);
    const nx_device_t* descriptor = NULL;
    assert(nx_device_discover("GPIOD12_R", NX_DEVICE_CLASS_GPIO_READ, &descriptor) == NX_OK);
    assert(nx_device_discover("GPIOD13_W", NX_DEVICE_CLASS_GPIO_WRITE, &descriptor) == NX_OK);
    assert(nx_device_discover("GPIOD14", NX_DEVICE_CLASS_GPIO, &descriptor) == NX_OK);
    assert(clocks == 0 && initialized == 0);
    assert(nx_factory_gpio_read('D', 12) && nx_factory_gpio_write('D', 13));
    assert(clocks == 0 && initialized == 0); /* Factories only bind static storage. */
    nx_device_ref_t reader = {0}, writer = {0}, rw = {0};
    assert(nx_device_open("GPIOD12_R", NX_DEVICE_CLASS_GPIO, 1, &reader) == NX_ERR_TYPE_MISMATCH);
    assert(nx_device_open("GPIOD12_R", NX_DEVICE_CLASS_GPIO_READ, 1, &reader) == NX_OK);
    assert(nx_device_open("GPIOD13_W", NX_DEVICE_CLASS_GPIO_WRITE, 2, &writer) == NX_OK);
    assert(nx_device_open("GPIOD14", NX_DEVICE_CLASS_GPIO, 3, &rw) == NX_OK);
    assert(initialized == 3);
    uint8_t value = 0;
    assert(nx_device_gpio_write(reader, 1) == NX_ERR_NOT_SUPPORTED);
    assert(nx_device_gpio_read(writer, &value) == NX_ERR_NOT_SUPPORTED);
    GPIOD->value |= GPIO_PIN_12;
    assert(nx_device_gpio_read(reader, &value) == NX_OK && value == 1);
    assert(nx_device_gpio_write(writer, 1) == NX_OK && (GPIOD->value & GPIO_PIN_13));
    assert(nx_device_gpio_toggle(rw) == NX_OK && (GPIOD->value & GPIO_PIN_14));
    nx_device_ref_t old = rw;
    assert(nx_device_close(rw) == NX_OK);
    assert(nx_device_open("GPIOD14", NX_DEVICE_CLASS_GPIO, 3, &rw) == NX_OK);
    assert(nx_device_gpio_toggle(old) == NX_ERR_INVALID_STATE);
    assert(nx_device_close(reader) == NX_OK && nx_device_close(writer) == NX_OK && nx_device_close(rw) == NX_OK);
    assert(initialized == 0 && nx_device_registry_reset() == NX_OK);
    puts("STM32 GPIO registered read/write/RW classes and static ownership: passed");
    return 0;
}

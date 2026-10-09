/** GPIO registration config reaches production hardware init unchanged. */
#include "hal/base/nx_device.h"
#include "hal/provider/nx_device_provider.h"
#include "stm32_gpio.h"
#include <assert.h>
#include <stdio.h>

GPIO_TypeDef fake_gpio_a, fake_gpio_d;
static unsigned clocks, initialized;
uint32_t __get_IPSR(void) { return 0; }
void fake_gpio_clock(GPIO_TypeDef* port) { assert(port == GPIOD); ++clocks; }
void HAL_GPIO_Init(GPIO_TypeDef* port, GPIO_InitTypeDef* gpio) {
    assert(port == GPIOD && clocks);
    if (gpio->Pin == GPIO_PIN_12)
        assert(gpio->Mode == GPIO_MODE_AF_PP && gpio->Alternate == 5);
    else if (gpio->Pin == GPIO_PIN_13)
        assert(gpio->Mode == GPIO_MODE_AF_OD && gpio->Alternate == 7);
    else {
        assert(gpio->Pin == GPIO_PIN_14 && gpio->Mode == GPIO_MODE_OUTPUT_PP);
        assert(gpio->Alternate == 0 && (port->value & GPIO_PIN_14));
    }
    ++initialized;
}
void HAL_GPIO_DeInit(GPIO_TypeDef* port, uint32_t pin) { assert(port == GPIOD && pin); --initialized; }
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState value) {
    if (value) port->value |= pin; else port->value &= (uint16_t)~pin;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin) { return port->value & pin ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin) { port->value ^= pin; }

int main(void) {
    extern const nx_device_t STM32_GPIOD12, STM32_GPIOD13, STM32_GPIOD14;
    const nx_device_t* descriptors[] = {&STM32_GPIOD12, &STM32_GPIOD13, &STM32_GPIOD14};
    for (unsigned i = 0; i < 3; ++i) {
        void* api = NULL;
        assert(descriptors[i]->construct(descriptors[i], &api) == NX_OK && api);
        nx_gpio_write_t* writer = i < 2 ? &((nx_gpio_read_write_t*)api)->write : api;
        nx_lifecycle_t* lifecycle = writer->get_lifecycle(writer);
        assert(lifecycle->init(lifecycle) == NX_OK);
        assert(lifecycle->deinit(lifecycle) == NX_OK);
    }
    assert(clocks == 3 && initialized == 0);
    const stm32_gpio_config_t invalid = {.port=GPIOD, .pin=GPIO_PIN_12,
        .mode=GPIO_MODE_AF_PP, .alternate=16};
    stm32_gpio_state_t state = {.config=&invalid};
    assert(stm32_gpio_hw_init(&state) == NX_ERR_INVALID_PARAM);
    assert(clocks == 3 && initialized == 0 && !state.initialized);
    puts("STM32 config AF5/AF7, ordinary GPIO and invalid alternate rejection passed");
    return 0;
}

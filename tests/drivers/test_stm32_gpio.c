#include "stm32_gpio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void stm32_gpio_init_lifecycle_read(nx_lifecycle_t*);
void stm32_gpio_init_lifecycle_write(nx_lifecycle_t*);
void stm32_gpio_init_lifecycle_read_write(nx_lifecycle_t*);
void stm32_gpio_init_power_write(nx_power_t*);
void stm32_gpio_init_read(nx_gpio_read_t*);
void stm32_gpio_init_write(nx_gpio_write_t*);
GPIO_TypeDef fake_gpio_a, fake_gpio_d;
static unsigned clocked, initialized, callback_count;
static uint32_t ipsr;
uint32_t __get_IPSR(void) { return ipsr; }
void fake_gpio_clock(GPIO_TypeDef* port) { assert(port==GPIOD); ++clocked; }
void HAL_GPIO_Init(GPIO_TypeDef* port, GPIO_InitTypeDef* gpio) {
    assert(clocked && port==GPIOD && gpio->Pin==GPIO_PIN_12);
    assert(port->value & GPIO_PIN_12); /* Output latch precedes mode switch. */
    ++initialized;
}
void HAL_GPIO_DeInit(GPIO_TypeDef* port, uint32_t pin) { assert(port==GPIOD && pin==GPIO_PIN_12); --initialized; }
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, GPIO_PinState value) {
    if (value) port->value |= pin; else port->value &= (uint16_t)~pin;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin) { return port->value & pin ? GPIO_PIN_SET : GPIO_PIN_RESET; }
void HAL_GPIO_TogglePin(GPIO_TypeDef* port, uint16_t pin) { port->value ^= pin; }
static void power_event(void* context, bool on) { assert(context==&callback_count); (void)on; ++callback_count; }
static void exti_event(void* context) { (void)context; }
int main(void) {
    const stm32_gpio_config_t config={.port=GPIOD,.pin=GPIO_PIN_12,
        .mode=GPIO_MODE_OUTPUT_PP,.init_value=1};
    stm32_gpio_state_t state={.config=&config,.port=GPIOD,.pin=GPIO_PIN_12};
    stm32_gpio_write_impl_t w={.state=&state};
    stm32_gpio_init_lifecycle_write(&w.lifecycle);
    stm32_gpio_init_power_write(&w.power);
    stm32_gpio_init_write(&w.base);
    assert(w.lifecycle.init(&w.lifecycle)==NX_OK && clocked==1 && initialized==1);
    assert(w.lifecycle.init(&w.lifecycle)==NX_ERR_ALREADY_INIT);
    assert(w.lifecycle.get_state(&w.lifecycle)==NX_DEV_STATE_RUNNING);
    assert(w.power.set_callback(&w.power,power_event,&callback_count)==NX_OK);
    w.base.write(&w.base,0); assert(!(GPIOD->value & GPIO_PIN_12));
    assert(w.power.disable(&w.power)==NX_OK && !w.power.is_enabled(&w.power));
    w.base.write(&w.base,1); assert(!(GPIOD->value & GPIO_PIN_12));
    assert(w.lifecycle.get_state(&w.lifecycle)==NX_DEV_STATE_SUSPENDED);
    assert(w.power.enable(&w.power)==NX_OK && callback_count==2);
    w.base.toggle(&w.base); assert(GPIOD->value & GPIO_PIN_12);
    assert(w.lifecycle.deinit(&w.lifecycle)==NX_OK && initialized==0);
    assert(w.lifecycle.get_state(&w.lifecycle)==NX_DEV_STATE_UNINITIALIZED);
    assert(w.power.enable(&w.power)==NX_ERR_NOT_INIT);
    ipsr=16; assert(w.lifecycle.init(&w.lifecycle)==NX_ERR_INVALID_STATE); ipsr=0;
    stm32_gpio_read_impl_t r={.state=&state};
    stm32_gpio_init_lifecycle_read(&r.lifecycle); stm32_gpio_init_read(&r.base);
    state.initialized=true;
    assert(r.base.register_exti(&r.base,exti_event,NULL,NX_GPIO_TRIGGER_RISING)==NX_ERR_NOT_SUPPORTED);
    assert(!state.exti.enabled);
    puts("STM32 GPIO contracts: clock, output ordering, full lifecycle/power, unsupported EXTI passed");
    return 0;
}

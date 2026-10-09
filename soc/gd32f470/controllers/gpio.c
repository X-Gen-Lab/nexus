#include "hal/provider/nx_device_provider.h"
#include "arch/nx_arch.h"
#include "board.h"
#include "gd32f4xx.h"
#include "hal/base/nx_device.h"
#include "hal/interface/nx_gpio.h"

typedef struct {
    nx_gpio_t api;
    nx_lifecycle_t lifecycle;
    nx_device_state_t state;
} gpio_instance_t;
static gpio_instance_t led;

static nx_status_t gpio_init_device(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    gpio_bit_reset(GPIOD, GPIO_PIN_7);
    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_7);
    led.state = NX_DEV_STATE_RUNNING;
    return NX_OK;
}
static nx_status_t gpio_deinit_device(nx_lifecycle_t* self) {
    (void)self;
    if (nx_arch_in_isr()) { return NX_ERR_INVALID_STATE; }
    gpio_bit_reset(GPIOD, GPIO_PIN_7);
    led.state = NX_DEV_STATE_UNINITIALIZED;
    return NX_OK;
}
static nx_status_t gpio_suspend(nx_lifecycle_t* self) {
    if (led.state != NX_DEV_STATE_RUNNING) { return NX_ERR_INVALID_STATE; }
    nx_status_t status = gpio_deinit_device(self);
    if (status == NX_OK) { led.state = NX_DEV_STATE_SUSPENDED; }
    return status;
}
static nx_status_t gpio_resume(nx_lifecycle_t* self) {
    return led.state == NX_DEV_STATE_SUSPENDED ? gpio_init_device(self)
                                              : NX_ERR_INVALID_STATE;
}
static nx_device_state_t gpio_state(nx_lifecycle_t* self) { (void)self; return led.state; }
static uint8_t gpio_read(nx_gpio_read_t* self) {
    (void)self;
    return (uint8_t)(led.state == NX_DEV_STATE_RUNNING &&
                     gpio_input_bit_get(GPIOD, GPIO_PIN_7) != RESET);
}
static void gpio_write(nx_gpio_write_t* self, uint8_t value) {
    (void)self;
    if (led.state == NX_DEV_STATE_RUNNING) { gpio_bit_write(GPIOD, GPIO_PIN_7, value ? SET : RESET); }
}
static void gpio_toggle(nx_gpio_write_t* self) {
    (void)self;
    if (led.state != NX_DEV_STATE_RUNNING) { return; }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    gpio_bit_write(GPIOD, GPIO_PIN_7, gpio_output_bit_get(GPIOD, GPIO_PIN_7) ? RESET : SET);
    nx_arch_irq_restore(saved);
}
static nx_status_t gpio_exti(nx_gpio_read_t* self, nx_gpio_callback_t callback,
                             void* context, nx_gpio_trigger_t trigger) {
    (void)self; (void)callback; (void)context; (void)trigger;
    return NX_ERR_NOT_SUPPORTED;
}
static nx_lifecycle_t* gpio_read_lifecycle(nx_gpio_read_t* self) { (void)self; return &led.lifecycle; }
static nx_lifecycle_t* gpio_write_lifecycle(nx_gpio_write_t* self) { (void)self; return &led.lifecycle; }
static nx_power_t* gpio_read_power(nx_gpio_read_t* self) { (void)self; return NULL; }
static nx_power_t* gpio_write_power(nx_gpio_write_t* self) { (void)self; return NULL; }
static void* create_gpio(const nx_device_t* descriptor) {
    (void)descriptor;
    led.lifecycle = (nx_lifecycle_t){ .init = gpio_init_device, .deinit = gpio_deinit_device,
        .suspend = gpio_suspend, .resume = gpio_resume, .get_state = gpio_state };
    NX_INIT_GPIO_READ_WRITE(&led.api, gpio_read, gpio_exti, gpio_write, gpio_toggle,
        gpio_read_lifecycle, gpio_read_power, gpio_write_lifecycle, gpio_write_power);
    return &led.api;
}
static nx_device_config_state_t device_state;
NX_DEVICE_REGISTER(NX_GPIO, D7, "GPIOD7", NULL, &device_state, create_gpio);

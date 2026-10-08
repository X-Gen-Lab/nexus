#include "board.h"
#include "gd32f4xx.h"
#include "nexus_config.h"

static void configure_output(uint32_t port, uint32_t pins, uint32_t speed) {
    gpio_mode_set(port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pins);
    gpio_output_options_set(port, GPIO_OTYPE_PP, speed, pins);
}
nx_status_t nx_gd32_board_safe_init(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOF);
    gpio_bit_reset(GPIOD, GPIO_PIN_7);
    gpio_bit_set(GPIOF, GPIO_PIN_6);
    configure_output(GPIOD, GPIO_PIN_7, GPIO_OSPEED_2MHZ);
    configure_output(GPIOF, GPIO_PIN_6, GPIO_OSPEED_2MHZ);
#ifdef NX_CONFIG_GD32_RS485_ENABLE
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_bit_reset(GPIOB, GPIO_PIN_1);
    configure_output(GPIOB, GPIO_PIN_1, GPIO_OSPEED_2MHZ);
#endif
    return NX_OK;
}
nx_status_t nx_gd32_board_uart_pins(bool enable) {
    if (enable) {
        gpio_bit_set(GPIOA, GPIO_PIN_9);
        gpio_af_set(GPIOA, GPIO_AF_7, GPIO_PIN_9 | GPIO_PIN_10);
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                                GPIO_PIN_9 | GPIO_PIN_10);
    } else {
        gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_9 | GPIO_PIN_10);
    }
    nx_gd32_board_rs485_de(false);
    return NX_OK;
}
void nx_gd32_board_rs485_de(bool active) {
#ifdef NX_CONFIG_GD32_RS485_ENABLE
    gpio_bit_write(GPIOB, GPIO_PIN_1, active ? SET : RESET);
#else
    (void)active;
#endif
}
nx_status_t nx_gd32_board_spi_pins(bool enable) {
    gpio_bit_set(GPIOF, GPIO_PIN_6);
    gpio_af_set(GPIOF, GPIO_AF_5, GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOF, enable ? GPIO_MODE_AF : GPIO_MODE_INPUT, GPIO_PUPD_NONE,
                  GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    return NX_OK;
}
nx_status_t nx_gd32_board_spi_cs(uint8_t logical, bool active) {
    if (logical != 0u) { return NX_ERR_INVALID_PARAM; }
    gpio_bit_write(GPIOF, GPIO_PIN_6, active ? RESET : SET);
    return NX_OK;
}

#include "nexus_board.h"
#include "stm32_uart_resource.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdio.h>

GPIO_TypeDef fixture_a, fixture_b, fixture_e, fixture_g;
static unsigned output_calls, alternate_calls, uart_clock;

void fixture_clock(GPIO_TypeDef* port) { ++port->clocks; }
void fixture_uart_clock(unsigned enabled) { uart_clock = enabled; }
void HAL_GPIO_WritePin(GPIO_TypeDef* port, uint16_t pins, GPIO_PinState level) {
    assert(port->clocks); /* The peripheral clock precedes the latch write. */
    if (level) port->latch |= pins;
    else port->latch &= ~pins;
}
void HAL_GPIO_Init(GPIO_TypeDef* port, GPIO_InitTypeDef* pins) {
    assert(port->clocks);
    if (pins->Mode == GPIO_MODE_OUTPUT_PP) {
        assert(pins->Pull == GPIO_NOPULL && pins->Speed == GPIO_SPEED_FREQ_LOW);
        /* Validate physical inactive latch before any output mode switch. */
#if defined(TEST_QIMING_BOARD)
        assert((port == GPIOE && pins->Pin == (GPIO_PIN_3 | GPIO_PIN_4)) ||
               (port == GPIOG && pins->Pin == GPIO_PIN_9));
        assert((port->latch & pins->Pin) == pins->Pin);
#else
        assert(port == GPIOB && pins->Pin == GPIO_PIN_2);
        assert((port->latch & pins->Pin) == 0);
#endif
        port->outputs |= pins->Pin;
        ++output_calls;
    } else {
        assert(port == GPIOA && pins->Pin == (GPIO_PIN_9 | GPIO_PIN_10));
        assert(pins->Mode == GPIO_MODE_AF_PP && pins->Alternate == GPIO_AF7_USART1);
        assert(pins->Pull == GPIO_PULLUP && uart_clock == 1);
        port->alternate |= pins->Pin;
        ++alternate_calls;
    }
}
void HAL_GPIO_DeInit(GPIO_TypeDef* port, uint32_t pins) {
    assert(port == GPIOA && pins == (GPIO_PIN_9 | GPIO_PIN_10));
    port->alternate &= ~pins;
}

int main(void) {
    /* Seed an unsafe LED latch so forgetting the preload fails this fixture. */
    fixture_b.latch = GPIO_PIN_2;
    HAL_MspInit();
#if defined(TEST_QIMING_BOARD)
    assert(output_calls == 2 && GPIOE->outputs == (GPIO_PIN_3 | GPIO_PIN_4));
    assert(GPIOG->outputs == GPIO_PIN_9);
    assert(NX_BOARD_LED_ACTIVE_LEVEL == 0 && NX_BOARD_LED_INACTIVE_LEVEL == 1);
    assert(GPIOB->outputs == 0);
#else
    assert(output_calls == 1 && GPIOB->outputs == GPIO_PIN_2);
    assert(NX_BOARD_LED_ACTIVE_LEVEL == 1 && NX_BOARD_LED_INACTIVE_LEVEL == 0);
    assert(GPIOE->outputs == 0 && GPIOG->outputs == 0);
#endif
    assert(GPIOA->outputs == 0 && alternate_calls == 0);
    puts("Early board MSP preloads inactive LEDs and leaves unrelated outputs untouched");
    assert(stm32_uart_board_prepare(1) == NX_ERR_NOT_SUPPORTED);
    assert(uart_clock == 0 && alternate_calls == 0);
    assert(stm32_uart_board_prepare(0) == NX_OK);
    assert(alternate_calls == 1 && uart_clock == 1);
    assert(stm32_uart_board_direction(0, true) == NX_ERR_NOT_SUPPORTED);
    assert(stm32_uart_board_direction(0, false) == NX_OK);
    assert(stm32_uart_board_direction(1, false) == NX_ERR_NOT_SUPPORTED);
    assert(stm32_uart_board_release(1) == NX_ERR_NOT_SUPPORTED);
    assert(stm32_uart_board_release(0) == NX_OK);
    assert(uart_clock == 0 && GPIOA->alternate == 0 && GPIOA->clocks == 1);
    puts("UART0 resource bind/release, unsupported controller and no invented DE passed");
    return 0;
}

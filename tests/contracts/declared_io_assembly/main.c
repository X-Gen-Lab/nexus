/**
 * \file            main.c
 * \brief           Software-only exact-part I/O selection link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-03
 * \copyright       Copyright (c) 2026 Nexus Team
 * \details         Retain selected typed methods without submitting requests.
 *                  ARM linking does not establish execution or physical wiring.
 */
#include "nexus/io/adc.h"
#include "nexus/io/can.h"
#include "nexus/io/exti.h"
#include "nexus/io/gpio.h"
#include "nexus/io/uart.h"
#include "nexus/io/uart_word.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"

/* Volatile face reads retain the real operations and all non-NULL methods
 * through section GC. No fixture duplicates a controller or provider registry.
 */
#define NX_DECLARED_RETAIN_FACES(name, type, count)                            \
    do {                                                                       \
        size_t instance_count = (count);                                       \
        for (size_t index = 0; index < instance_count; ++index) {              \
            const volatile type* face =                                        \
                nx_factory_##name((nx_##name##_id_t)index);                    \
            if (face != NULL) {                                                \
                (void)face->ops;                                               \
                (void)face->context;                                           \
            }                                                                  \
        }                                                                      \
    } while (0)

/** \brief Retain the selected platform lifecycle and every typed face.
 */
int main(void) {
    nx_platform_start_result_t result = nx_platform_start();
    if (result.primary != NX_SUCCESS) {
        return 1;
    }
#ifdef NX_DECLARED_GPIO
    NX_DECLARED_RETAIN_FACES(gpio, nx_gpio_port_t, NX_GPIO_ID_COUNT);
#endif
#ifdef NX_DECLARED_EXTI
    NX_DECLARED_RETAIN_FACES(exti, nx_exti_port_t, NX_EXTI_ID_COUNT);
#endif
#ifdef NX_DECLARED_UART
    NX_DECLARED_RETAIN_FACES(uart, nx_uart_port_t, NX_UART_ID_COUNT);
#endif
#ifdef NX_DECLARED_UART_WORD
    NX_DECLARED_RETAIN_FACES(uart_word, nx_uart_word_port_t,
                             NX_UART_WORD_ID_COUNT);
#endif
#ifdef NX_DECLARED_ADC
    NX_DECLARED_RETAIN_FACES(adc, nx_adc_port_t, NX_ADC_ID_COUNT);
#endif
#ifdef NX_DECLARED_CAN
    NX_DECLARED_RETAIN_FACES(can, nx_can_port_t, NX_CAN_ID_COUNT);
#endif
    for (;;) {
    }
}

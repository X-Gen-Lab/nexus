/**
 * \file            main.c
 * \brief           Exact F100RB Board-owned PB8 CEC exchange fixture.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/cec/instance.h"
#include "nexus_bindings.h"

static nx_stm32f1_cec_t cec;

void CEC_IRQHandler(void);

void CEC_IRQHandler(void) {
    nx_stm32f1_cec_irq(&cec);
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    nx_result_t result = nx_stm32f1_cec_prepare(&cec, 4U, 5U);
    nx_time_us_t deadline;
    if (result == NX_SUCCESS)
        result = nx_deadline_finite_after(nx_time_now_us(), 500000U, &deadline);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_cec_send(&cec, 0U, NULL, 0U);
    if (result == NX_SUCCESS) {
        do {
            result = nx_stm32f1_cec_transmit_status(&cec);
        } while (result == NX_ERROR_BUSY &&
                 !nx_deadline_expired(deadline, nx_time_now_us()));
        uint8_t receive[NX_STM32F1_CEC_FRAME_BYTES];
        size_t bytes;
        (void)nx_stm32f1_cec_receive(&cec, receive, sizeof(receive), &bytes);
    }
    if (nx_deadline_finite_after(nx_time_now_us(), 500000U, &deadline) !=
            NX_SUCCESS ||
        nx_stm32f1_cec_close(&cec, deadline) != NX_SUCCESS)
        return 2;
    return nx_platform_stop() == NX_SUCCESS &&
                   (result == NX_SUCCESS || result == NX_ERROR_NACK)
               ? 0
               : 3;
}

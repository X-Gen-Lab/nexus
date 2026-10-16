/**
 * \file            nonsecure.c
 * \brief           Unprivileged single-world scalar gateway reference caller
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gateway.h"
static nx_user_io_message_t message = {
    NX_USER_IO_ABI, 1U, NX_USER_GPIO_WRITE, 0x80U, 0U, 0U, {0U, 0U}};
static nx_user_io_reply_t reply;
static volatile uint32_t result;
static nx_stm32u575_secure_batch_message_t batch_message = {
    .action = NX_STM32U575_BATCH_SUBMIT,
    .count = 2U,
    .commands = {{0x80U, 0U}, {0U, 0x80U}}};
static nx_stm32u575_secure_batch_reply_t batch_reply;
void nx_reference_nonsecure_main(void);

void nx_reference_nonsecure_main(void) {
    result = nx_reference_gpio(&message, &reply);
    result = nx_reference_gpio_batch(&batch_message, &batch_reply);
    if (result == (uint32_t)NX_SUCCESS) {
        batch_message.ticket = batch_reply.ticket;
        batch_message.count = 0U;
        batch_message.action = NX_STM32U575_BATCH_SERVICE;
        result = nx_reference_gpio_batch(&batch_message, &batch_reply);
        if (result == (uint32_t)NX_SUCCESS)
            result = nx_reference_gpio_batch(&batch_message, &batch_reply);
        batch_message.action = NX_STM32U575_BATCH_POLL;
        result = nx_reference_gpio_batch(&batch_message, &batch_reply);
        batch_message.action = NX_STM32U575_BATCH_CONSUME;
        result = nx_reference_gpio_batch(&batch_message, &batch_reply);
    }
    for (;;) {
        __asm volatile("wfi" ::: "memory");
    }
}

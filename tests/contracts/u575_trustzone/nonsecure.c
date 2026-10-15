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
void nx_reference_nonsecure_main(void);

void nx_reference_nonsecure_main(void) {
    result = nx_reference_gpio(&message, &reply);
    for (;;) {
        __asm volatile("wfi" ::: "memory");
    }
}

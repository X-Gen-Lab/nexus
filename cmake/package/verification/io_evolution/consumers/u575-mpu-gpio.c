/**
 * \file            u575-mpu-gpio.c
 * \brief           Public normal-reset U575 MPU-v2 scalar GPIO consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/os/freertos_mpu_gpio.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "nexus_factory.h"

static StaticTask_t s_idle;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];
static nx_freertos_mpu_task_t s_user_task;
static nx_freertos_mpu_gpio_t s_authority;
static StackType_t s_user_stack[256] NX_FREERTOS_USER_BSS
    __attribute__((aligned(32)));
static volatile uint32_t s_result[8] NX_FREERTOS_USER_DATA
    __attribute__((aligned(32))) = {0x13579bdfU};
static const char s_name[configMAX_TASK_NAME_LEN] = "gpio";
static const nx_freertos_mpu_gpio_grant_t s_grant = {
    .port_slot = &nx_binding_led, .device = NX_GPIO_ID_LED, .mask = 0x80U};
const nx_freertos_mpu_gpio_binding_t nx_freertos_mpu_gpio_binding = {
    .control = &s_authority, .task = &s_user_task, .grant = &s_grant};

/** \brief The consumer supplies all real kernel Idle storage explicitly. */
void vApplicationGetIdleTaskMemory(StaticTask_t** control, StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* words) {
    *control = &s_idle;
    *stack = s_idle_stack;
    *words = configMINIMAL_STACK_SIZE;
}

static void user_entry(void* context) NX_FREERTOS_USER_CODE;
static void user_entry(void* context) {
    (void)context;
    s_result[0] =
        (uint32_t)nx_freertos_user_gpio_write(NX_GPIO_ID_LED, 0x80U, 0U);
    for (;;) {
        s_result[1] = nx_freertos_user_ticks();
        nx_freertos_user_delay(1U);
    }
}

int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS) {
        return 1;
    }
    if (nx_factory_gpio(NX_GPIO_ID_LED) != nx_binding_led) {
        return 5;
    }
    const nx_freertos_mpu_region_t region = {(void*)s_result, sizeof(s_result),
                                             NX_FREERTOS_MPU_READ_WRITE};
    const nx_freertos_mpu_config_t config = {
        .entry = user_entry,
        .context = NULL,
        .context_bytes = 0U,
        .name = s_name,
        .stack = s_user_stack,
        .stack_words = sizeof(s_user_stack) / sizeof(s_user_stack[0]),
        .priority = 1U,
        .regions = &region,
        .region_count = 1U};
    if (nx_freertos_mpu_task_start(&s_user_task, &config) != NX_SUCCESS) {
        return 2;
    }
    if (nx_freertos_mpu_gpio_open() != NX_SUCCESS) {
        (void)nx_freertos_mpu_gpio_close();
        (void)nx_freertos_mpu_gpio_join();
        (void)nx_freertos_mpu_task_delete(&s_user_task);
        return 3;
    }
    vTaskStartScheduler();
    return 4;
}

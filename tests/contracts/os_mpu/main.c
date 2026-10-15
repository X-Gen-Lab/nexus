/**
 * \file            main.c
 * \brief           Retained real MPU static task and narrow SVC link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "nexus/os/freertos.h"
#include "nexus/os/freertos_mpu.h"

void _start(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
static void (*const exception_references[])(void)
    __attribute__((used, section(".runtime.references"))) = {
        SVC_Handler, PendSV_Handler, SysTick_Handler};

#ifndef NEXUS_TEST_MPU_MISSING_IDLE_PROVIDER
/* The consumer owns the kernel's idle metadata and capacity explicitly. */
static StaticTask_t idle_task;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
void vApplicationGetIdleTaskMemory(StaticTask_t** control, StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* stack_words) {
    *control = &idle_task;
    *stack = idle_stack;
    *stack_words = configMINIMAL_STACK_SIZE;
}
#endif

static nx_freertos_mpu_task_t task NX_FREERTOS_PRIVILEGED_DATA;
static nx_freertos_permanent_task_t privileged_task;
static StackType_t privileged_stack[256];
static StackType_t user_stack[256] NX_FREERTOS_USER_DATA
    __attribute__((aligned(1024)));
static volatile uint32_t user_counter NX_FREERTOS_USER_DATA
    __attribute__((aligned(32)));
static const char task_name[] = "isolated";

static void user_entry(void* context) NX_FREERTOS_USER_CODE;
static void user_entry(void* context) {
    volatile uint32_t* counter = context;
    for (;;) {
        *counter = nx_freertos_user_ticks();
        nx_freertos_user_delay(1);
    }
}

static void privileged_entry(void* context) {
    (void)context;
    for (;;) {
    }
}

nx_time_us_t nx_time_now_us(void) {
    return 0;
}

void _start(void) {
    __asm__ volatile(".global __nexus_os_mpu_task_size\n"
                     ".equ __nexus_os_mpu_task_size,%c0\n"
                     ".global __nexus_os_mpu_tcb_size\n"
                     ".equ __nexus_os_mpu_tcb_size,%c1\n"
                     ".global __nexus_os_mpu_syscall_stack_bytes\n"
                     ".equ __nexus_os_mpu_syscall_stack_bytes,%c2\n"
                     :
                     : "i"(sizeof(task)), "i"(sizeof(StaticTask_t)),
                       "i"(configSYSTEM_CALL_STACK_SIZE * sizeof(uint32_t)));
    const nx_freertos_mpu_region_t region = {(void*)&user_counter, 32,
                                             NX_FREERTOS_MPU_READ_WRITE};
    const nx_freertos_mpu_config_t config = {user_entry,
                                             (void*)&user_counter,
                                             sizeof(user_counter),
                                             task_name,
                                             user_stack,
                                             256,
                                             1,
                                             &region,
                                             1};
    (void)nx_freertos_mpu_task_start(&task, &config);
    (void)nx_freertos_mpu_task_delete(&task);
    (void)nx_freertos_mpu_task_start(&task, &config);
    (void)nx_freertos_permanent_task_start(&privileged_task, "owner",
                                           privileged_stack, 256, 2,
                                           privileged_entry, NULL);
    vTaskStartScheduler();
    for (;;) {
    }
}

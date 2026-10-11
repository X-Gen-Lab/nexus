/**
 * \file            nonsecure.c
 * \brief           Real static NS kernel and Secure context gateway consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/freertos.h"
static StaticTask_t task_tcb
    __attribute__((section(".fixture.tcb"), aligned(8)));
static _Alignas(8) StackType_t task_stack[256];
static StaticTask_t idle_tcb
    __attribute__((section(".fixture.idle_tcb"), aligned(8)));
static _Alignas(8) StackType_t idle_stack[configMINIMAL_STACK_SIZE];
static volatile uint32_t result_sink;
void _start(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
/* Retain actual handlers without claiming this is a silicon vector table. */
static void (*const context_handlers[])(void)
    __attribute__((used, section(".fixture.handlers"))) = {
        SVC_Handler, PendSV_Handler, SysTick_Handler};
void vApplicationGetIdleTaskMemory(StaticTask_t** tcb, StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* words);

void vApplicationGetIdleTaskMemory(StaticTask_t** tcb, StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* words) {
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *words = configMINIMAL_STACK_SIZE;
}

static void context_task(void* unused) {
    (void)unused;
    portALLOCATE_SECURE_CONTEXT(256);
    for (;;) {
        vTaskDelay(1);
    }
}

void _start(void) {
    TaskHandle_t task = xTaskCreateStatic(context_task, "context", 256, NULL, 1,
                                          task_stack, &task_tcb);
    result_sink = task != NULL;
    vTaskStartScheduler();
    for (;;) {
    }
}

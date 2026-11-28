/**
 * \file            master_irq.c
 * \brief           F103RB clock, pad and vector ownership for the IRQ writer.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/i2c/master_irq.h"
#include "private/hardware.h"

#if !defined(STM32F103xB) || !defined(NX_I2C_V1_F1_WRITE_ONLY)
#error "The IRQ writer requires the exact reviewed F103RB I2C1 policy"
#endif

_Static_assert(I2C1_EV_IRQn == 31 && I2C1_ER_IRQn == 32 &&
                   offsetof(I2C_TypeDef, CR2) == 1U * sizeof(uint32_t) &&
                   offsetof(I2C_TypeDef, DR) == 4U * sizeof(uint32_t) &&
                   offsetof(I2C_TypeDef, SR2) == 6U * sizeof(uint32_t) &&
                   offsetof(I2C_TypeDef, TRISE) == 8U * sizeof(uint32_t),
               "Exact shared V1 registers and IRQ vectors");

static const nx_i2c_v1_lines_t lines = {
    .inputs = {&GPIOB->IDR, &GPIOB->IDR},
    .masks = {GPIO_IDR_IDR6, GPIO_IDR_IDR7},
};

static bool gate(void) {
    return (READ_REG(RCC->APB1ENR) & RCC_APB1ENR_I2C1EN) != 0U;
}

static bool quiet(IRQn_Type irq) {
    return NVIC_GetActive(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U;
}

static bool vectors_off(void) {
    return NVIC_GetEnableIRQ(I2C1_EV_IRQn) == 0U &&
           NVIC_GetEnableIRQ(I2C1_ER_IRQn) == 0U && quiet(I2C1_EV_IRQn) &&
           quiet(I2C1_ER_IRQn);
}

static void acknowledge(void* context) {
    (void)context;
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    nx_arch_dsb();
}

static nx_result_t stop_vectors(void* context) {
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    acknowledge(context);
    return vectors_off() ? NX_SUCCESS : NX_ERROR_BUSY;
}

bool nx_i2c_v1_f1_retire_local(nx_i2c_v1_state_t* state) {
    if (state->registers != &I2C1->CR1 || !gate()) {
        return false;
    }
    /* Hold local reset. External BUSY and pin levels belong to recovery. */
    WRITE_REG(I2C1->CR2, 0U);
    WRITE_REG(I2C1->CR1, I2C_CR1_SWRST);
    nx_arch_dsb();
    acknowledge(state);
    return gate() && READ_REG(I2C1->CR1) == I2C_CR1_SWRST &&
           READ_REG(I2C1->CR2) == 0U &&
           (READ_REG(I2C1->SR2) & I2C_SR2_MSL) == 0U && quiet(I2C1_EV_IRQn) &&
           quiet(I2C1_ER_IRQn);
}

nx_result_t nx_stm32f1_i2c_irq_open(nx_i2c_v1_state_t* port,
                                    nx_stm32f1_i2c_irq_cold_t* cold) {
    if (port == NULL || cold == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->registers != NULL || cold->vectors || !vectors_off()) {
        return NX_ERROR_BUSY;
    }
    const nx_result_t ready =
        nx_stm32f1_i2c_prepare(&cold->bus, 8000000U, 80000U);
    if (ready != NX_SUCCESS) {
        return ready;
    }
    if (!nx_i2c_v1_lines_high(&lines)) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_i2c_v1_state_t){.registers = &I2C1->CR1,
                                .lines = &lines,
                                .peripheral_mhz = 8U,
                                .timing_ccr = 50U,
                                .ack_irqs = acknowledge,
                                .stop_irqs = stop_vectors};
    const nx_result_t initialized = nx_i2c_v1_initialize(port);
    if (initialized != NX_SUCCESS) {
        return initialized;
    }
    cold->before_event = (uint8_t)NVIC_GetPriority(I2C1_EV_IRQn);
    cold->before_error = (uint8_t)NVIC_GetPriority(I2C1_ER_IRQn);
    cold->vectors = true;
    NVIC_SetPriority(I2C1_EV_IRQn, 0U);
    NVIC_SetPriority(I2C1_ER_IRQn, 0U);
    if (NVIC_GetPriority(I2C1_EV_IRQn) != 0U ||
        NVIC_GetPriority(I2C1_ER_IRQn) != 0U) {
        return NX_ERROR_IO;
    }
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
    return NVIC_GetEnableIRQ(I2C1_EV_IRQn) != 0U &&
                   NVIC_GetEnableIRQ(I2C1_ER_IRQn) != 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

nx_result_t nx_stm32f1_i2c_irq_close(nx_i2c_v1_state_t* port,
                                     nx_stm32f1_i2c_irq_cold_t* cold) {
    if (port == NULL || cold == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->initialized) {
        if (!gate()) {
            return NX_ERROR_IO;
        }
        const nx_result_t stopped = nx_i2c_v1_async_ops.stop(port);
        if (stopped != NX_SUCCESS) {
            return stopped;
        }
    }
    if (cold->vectors) {
        if (!vectors_off()) {
            return NX_ERROR_BUSY;
        }
        NVIC_SetPriority(I2C1_EV_IRQn, cold->before_event);
        NVIC_SetPriority(I2C1_ER_IRQn, cold->before_error);
        if (NVIC_GetPriority(I2C1_EV_IRQn) != cold->before_event ||
            NVIC_GetPriority(I2C1_ER_IRQn) != cold->before_error) {
            return NX_ERROR_IO;
        }
        cold->vectors = false;
    }
    const nx_result_t closed = nx_stm32f1_i2c_close(&cold->bus);
    if (closed == NX_SUCCESS) {
        port->registers = NULL;
    }
    return closed;
}

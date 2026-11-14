/**
 * \file            gpio.c
 * \brief           GD32 GPIO authorization using the official SPL operations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32_reference.h"
#include "nexus/arch/arch.h"

/** \brief Expand the selected pins to their independent two-bit fields. */
static uint32_t mode_mask(uint32_t mask) {
    uint32_t fields = 0U;
    for (unsigned pin = 0U; pin < 16U; ++pin) {
        if ((mask & (UINT32_C(1) << pin)) != 0U)
            fields |= UINT32_C(3) << (2U * pin);
    }
    return fields;
}

/** \brief Admit cold state only; GPIO I/O itself remains task/IRQ bounded. */
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

nx_result_t nx_gd32_gpio_admit(nx_gd32_gpio_state_t* state,
                               const nx_gd32_gpio_config_t* config) {
    if (state == NULL || config == NULL || config->mask == 0U ||
        (config->mask & ~UINT32_C(0xffff)) != 0U ||
        (config->initial & ~config->mask) != 0U || config->registers < GPIOA ||
        config->registers > GPIOI || (config->registers - GPIOA) % 0x400U != 0U)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (state->config != NULL)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

nx_result_t nx_gd32_gpio_prepare(nx_gd32_gpio_state_t* state,
                                 const nx_gd32_gpio_config_t* config) {
    nx_result_t result = nx_gd32_gpio_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    uint32_t port = config->registers;
    uint32_t bank = (port - GPIOA) / 0x400U;
    if ((RCU_AHB1EN & (UINT32_C(1) << bank)) == 0U)
        return NX_ERROR_STATE;
    if ((GPIO_LOCK(port) & GPIO_LOCK_LKK) != 0U &&
        (GPIO_LOCK(port) & config->mask) != 0U)
        return NX_ERROR_BUSY;
    uint32_t fields = mode_mask(config->mask);
    state->saved_mode = GPIO_CTL(port) & fields;
    state->saved_type = GPIO_OMODE(port) & config->mask;
    state->saved_speed = GPIO_OSPD(port) & fields;
    state->saved_pull = GPIO_PUD(port) & fields;
    state->saved_output = GPIO_OCTL(port) & config->mask;
    state->config = config;
    /* SPL has separate set/reset writes. This single BOP store preserves the
     * public atomic mixed-mask contract and precedes enabling output mode. */
    GPIO_BOP(port) =
        config->initial | ((config->mask & ~config->initial) << 16U);
    gpio_output_options_set(port, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                            config->mask);
    /* The public default preserves slew. SPL combines type and speed, so put
     * the captured selected speed fields back before enabling output mode. */
    GPIO_OSPD(port) = (GPIO_OSPD(port) & ~fields) | state->saved_speed;
    gpio_mode_set(port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, config->mask);
    nx_arch_dsb();
    if ((GPIO_CTL(port) & fields) != (fields & UINT32_C(0x55555555)) ||
        (GPIO_OMODE(port) & config->mask) != 0U ||
        (GPIO_OSPD(port) & fields) != state->saved_speed ||
        (GPIO_PUD(port) & fields) != 0U ||
        (GPIO_OCTL(port) & config->mask) != config->initial)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

nx_result_t nx_gd32_gpio_close(nx_gd32_gpio_state_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task())
        return NX_ERROR_CONTEXT;
    const nx_gd32_gpio_config_t* config = state->config;
    if (config == NULL)
        return NX_SUCCESS;
    uint32_t port = config->registers;
    if ((GPIO_LOCK(port) & GPIO_LOCK_LKK) != 0U &&
        (GPIO_LOCK(port) & config->mask) != 0U)
        return NX_ERROR_BUSY;
    uint32_t fields = mode_mask(config->mask);
    gpio_mode_set(port, GPIO_MODE_INPUT, GPIO_PUPD_NONE, config->mask);
    GPIO_BOP(port) =
        state->saved_output | ((config->mask & ~state->saved_output) << 16U);
    GPIO_OMODE(port) = (GPIO_OMODE(port) & ~config->mask) | state->saved_type;
    GPIO_OSPD(port) = (GPIO_OSPD(port) & ~fields) | state->saved_speed;
    GPIO_PUD(port) = (GPIO_PUD(port) & ~fields) | state->saved_pull;
    GPIO_CTL(port) = (GPIO_CTL(port) & ~fields) | state->saved_mode;
    nx_arch_dsb();
    if ((GPIO_CTL(port) & fields) != state->saved_mode ||
        (GPIO_OMODE(port) & config->mask) != state->saved_type ||
        (GPIO_OSPD(port) & fields) != state->saved_speed ||
        (GPIO_PUD(port) & fields) != state->saved_pull ||
        (GPIO_OCTL(port) & config->mask) != state->saved_output)
        return NX_ERROR_IO;
    state->config = NULL;
    return NX_SUCCESS;
}

/** \brief One atomic write constrained to the live immutable view. */
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    nx_gd32_gpio_state_t* state = context;
    if (state == NULL || state->config == NULL)
        return NX_ERROR_STATE;
    if ((set & reset) != 0U)
        return NX_ERROR_INVALID;
    if (((set | reset) & ~state->config->mask) != 0U)
        return NX_ERROR_PERMISSION;
    GPIO_BOP(state->config->registers) = set | (reset << 16U);
    return NX_SUCCESS;
}

/** \brief Copy only authorized input bits, preserving output on failure. */
static nx_result_t read(const void* context, uint32_t* value) {
    const nx_gd32_gpio_state_t* state = context;
    if (value == NULL)
        return NX_ERROR_INVALID;
    if (state == NULL || state->config == NULL)
        return NX_ERROR_STATE;
    *value =
        gpio_input_port_get(state->config->registers) & state->config->mask;
    return NX_SUCCESS;
}

/** \brief Reuse GD32's dedicated toggle register under the writer contract. */
static nx_result_t toggle(void* context, uint32_t mask) {
    nx_gd32_gpio_state_t* state = context;
    if (state == NULL || state->config == NULL)
        return NX_ERROR_STATE;
    if ((mask & ~state->config->mask) != 0U)
        return NX_ERROR_PERMISSION;
    gpio_bit_toggle(state->config->registers, mask);
    return NX_SUCCESS;
}

const nx_gpio_ops_t nx_gd32_gpio_ops = {
    .write = write, .read = read, .toggle = toggle};

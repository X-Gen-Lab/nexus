/**
 * \file            secure.c
 * \brief           Explicit single-principal reference gateway integration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gateway.h"
#include "nexus/arch/nonsecure.h"
#include "stm32u575_provider.h"
#include "stm32u575_security.h"
#include "user_io_broker_impl.h"
#include <arm_cmse.h>
#include <string.h>

static nx_stm32u575_tz_t owner;
static const nx_stm32u575_tz_layout_t layout = {
    0x08100000U, 0x10000U, 0x20040000U, 0x4000U,
    0x0c0fe000U, 0x2000U,  0x3002f000U, 0x1000U};
static nx_stm32u575_gpio_state_t led = {
    .registers = GPIOC, .mask = 0x80U, .output = true};
static const nx_gpio_port_t port = {&nx_stm32u575_gpio_ops, &led};
static nx_stm32u575_secure_batch_t batch = {.port = &port, .allowed = 0x80U};
static nx_user_io_broker_t broker;
static nx_user_io_call_t call;
/** \brief One immutable mapping epoch; two synchronous broker child loans. */
static struct {
    bool in_gateway;
    bool pinned[2];
} memory;
void nx_reference_secure_main(void);
_Noreturn void Secure_Fault_Handler(void);

/** \brief The checked SG caller belongs to the only permitted NS world. */
static nx_result_t principal(const void* context, uint32_t* value) {
    if (context != &memory || !memory.in_gateway ||
        owner.state != NX_STM32U575_TZ_RUNNING) {
        return NX_ERROR_CONTEXT;
    }
    *value = 1U;
    return NX_SUCCESS;
}

/** \brief Pin the sealed static RAM mapping for a synchronous call only. */
static nx_result_t pin(void* context, uint32_t identity, void* address,
                       size_t bytes, uint32_t access, uintptr_t* cookie) {
    uintptr_t first = (uintptr_t)address;
    if (context != &memory || !memory.in_gateway || identity != 1U ||
        owner.state != NX_STM32U575_TZ_RUNNING || bytes == 0U ||
        first < layout.ns_ram_base || bytes > layout.ns_ram_bytes ||
        first - layout.ns_ram_base > layout.ns_ram_bytes - bytes ||
        (access != NX_USER_MEMORY_READ && access != NX_USER_MEMORY_WRITE)) {
        return NX_ERROR_PERMISSION;
    }
    nx_arch_nonsecure_access_t requested = access == NX_USER_MEMORY_READ
                                               ? NX_ARCH_NONSECURE_READ
                                               : NX_ARCH_NONSECURE_READWRITE;
    if (nx_arch_nonsecure_check(address, bytes, requested) != NX_ARCH_OK) {
        return NX_ERROR_PERMISSION;
    }
    for (size_t index = 0U; index < 2U; ++index) {
        if (!memory.pinned[index]) {
            memory.pinned[index] = true;
            *cookie = index + 1U;
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_BUSY;
}

/** \brief Synchronous broker drain releases only the matching static loan. */
static nx_result_t unpin(void* context, uintptr_t cookie) {
    if (context != &memory || !memory.in_gateway || cookie < 1U ||
        cookie > 2U || !memory.pinned[cookie - 1U]) {
        return NX_ERROR_STATE;
    }
    memory.pinned[cookie - 1U] = false;
    return NX_SUCCESS;
}
static const nx_user_io_memory_ops_t memory_ops = {principal, pin, unpin};
static const nx_user_io_region_t region = {(void*)0x20040000U, 0x4000U, 1U,
                                           NX_USER_MEMORY_READ |
                                               NX_USER_MEMORY_WRITE};
static const nx_user_io_gpio_grant_t grant = {&port, sizeof(led), 1U,
                                              1U,    0x80U,       0x80U};
static const nx_user_io_broker_config_t config = {
    &region, 1U, &grant, 1U, 2U, {&memory_ops, &memory, sizeof(memory)}};

NX_REFERENCE_GATEWAY uint32_t nx_reference_gpio(
    const nx_user_io_message_t* message, nx_user_io_reply_t* reply) {
    if (!cmse_nonsecure_caller() || nx_arch_exception_number() != 0U ||
        !nx_arch_is_privileged() || nx_arch_irq_is_masked() ||
        owner.state != NX_STM32U575_TZ_RUNNING || memory.in_gateway) {
        return (uint32_t)NX_ERROR_CONTEXT;
    }
    uint32_t original;
    __asm volatile("mrs %0, primask_ns" : "=r"(original)::"memory");
    const uint32_t masked = 1U;
    __asm volatile("msr primask_ns, %0" : : "r"(masked) : "memory");
    nx_arch_dsb();
    nx_arch_isb();
    memory.in_gateway = true;
    nx_user_io_result_t result =
        nx_user_io_broker_gpio(&broker, &call, message, reply);
    if (result.retained || memory.pinned[0] || memory.pinned[1]) {
        Secure_Fault_Handler();
    }
    memory.in_gateway = false;
    __asm volatile("msr primask_ns, %0" : : "r"(original) : "memory");
    nx_arch_dsb();
    nx_arch_isb();
    return (uint32_t)result.result;
}

NX_REFERENCE_GATEWAY uint32_t
nx_reference_gpio_batch(const nx_stm32u575_secure_batch_message_t* message,
                        nx_stm32u575_secure_batch_reply_t* reply) {
    if (!cmse_nonsecure_caller() || nx_arch_exception_number() != 0U ||
        !nx_arch_is_privileged() || nx_arch_irq_is_masked() ||
        owner.state != NX_STM32U575_TZ_RUNNING || memory.in_gateway)
        return (uint32_t)NX_ERROR_CONTEXT;
    uint32_t original;
    __asm volatile("mrs %0, primask_ns" : "=r"(original)::"memory");
    const uint32_t masked = 1U;
    __asm volatile("msr primask_ns, %0" : : "r"(masked) : "memory");
    nx_arch_dsb();
    nx_arch_isb();
    memory.in_gateway = true;
    nx_result_t result = NX_ERROR_PERMISSION;
    /* The static NS world has no IRQ handlers, DMA or remapping authority.
     * Its complete immutable RAM mapping is checked before either copy.
     * CMSE permission checks remain mandatory even inside that address span. */
    if (nx_stm32u575_secure_batch_ns_spans(
            layout.ns_ram_base, layout.ns_ram_bytes, message, reply) &&
        nx_arch_nonsecure_check(message, sizeof(*message),
                                NX_ARCH_NONSECURE_READ) == NX_ARCH_OK &&
        nx_arch_nonsecure_check(reply, sizeof(*reply),
                                NX_ARCH_NONSECURE_READWRITE) == NX_ARCH_OK) {
        nx_stm32u575_secure_batch_message_t input;
        nx_stm32u575_secure_batch_reply_t output;
        memcpy(&input, message, sizeof(input));
        result = nx_stm32u575_secure_batch_dispatch(&batch, &input, &output);
        if (result == NX_SUCCESS)
            memcpy(reply, &output, sizeof(output));
    }
    memory.in_gateway = false;
    __asm volatile("msr primask_ns, %0" : : "r"(original) : "memory");
    nx_arch_dsb();
    nx_arch_isb();
    return (uint32_t)result;
}

void nx_reference_secure_main(void) {
    if (nx_stm32u575_tz_prepare(&owner, &layout) != NX_SUCCESS) {
        Secure_Fault_Handler();
    }
    /* The reference owns cold GPIOC but never declassifies another pin. */
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOCEN;
    nx_arch_dsb();
    if ((RCC->AHB2ENR1 & RCC_AHB2ENR1_GPIOCEN) == 0U ||
        (GPIOC->SECCFGR & 0x80U) == 0U) {
        Secure_Fault_Handler();
    }
    /* The cold boot owner releases its initial assembly mask exactly once. */
    __set_PRIMASK(0U);
    nx_arch_dsb();
    nx_arch_isb();
    if (nx_stm32u575_gpio_initialize(&led, 0U) != NX_SUCCESS ||
        nx_user_io_broker_construct(&broker, &config) != NX_SUCCESS) {
        Secure_Fault_Handler();
    }
    (void)nx_arch_irq_save();
    (void)nx_stm32u575_tz_launch(&owner);
    Secure_Fault_Handler();
}

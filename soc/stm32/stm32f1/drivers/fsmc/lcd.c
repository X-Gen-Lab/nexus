/**
 * \file            lcd.c
 * \brief           Settled halfword I/O on the selected MB672 CN16 bank.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_fsmc.h"
#include "system/platform.h"

#ifndef NX_STM32F1_FSMC_WRITE16
#define NX_STM32F1_FSMC_WRITE16(address, word)                                 \
    (*(volatile uint16_t*)(address) = (word))
#define NX_STM32F1_FSMC_READ16(address) (*(const volatile uint16_t*)(address))
#endif

static nx_result_t ready(const nx_stm32f1_fsmc_t* bus, nx_time_us_t deadline) {
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY;
    if (!bus->prepared || !nx_stm32f1_system_live() || (RCC->CR & hsi) != hsi ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE)) != 0U) {
        return NX_ERROR_STATE;
    }
    if ((RCC->AHBENR & RCC_AHBENR_FSMCEN) == 0U ||
        (FSMC_Bank1->BTCR[FSMC_NORSRAM_BANK4] & FSMC_BCRx_MBKEN) == 0U) {
        return NX_ERROR_IO;
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}

static nx_result_t command(void* context, uint16_t value,
                           nx_time_us_t deadline) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    nx_result_t result = ready(context, deadline);
    if (result != NX_SUCCESS)
        return result;
    NX_STM32F1_FSMC_WRITE16((uintptr_t)NX_STM32F1_FSMC_LCD_COMMAND, value);
    nx_arch_dsb();
    return ready(context, deadline);
}

static bool span(uintptr_t start, size_t bytes, uintptr_t origin, size_t size) {
    return start >= origin && bytes <= size && start - origin <= size - bytes;
}

static bool internal_storage(const nx_stm32f1_fsmc_t* bus, const void* data,
                             size_t bytes, bool writable) {
    const uintptr_t address = (uintptr_t)data;
    const uintptr_t state = (uintptr_t)bus;
    bool sram = span(address, bytes, SRAM_BASE, 96U * 1024U);
#if UINTPTR_MAX > UINT32_MAX
    sram = sram || address > UINT32_MAX;
#endif
    if (sram)
        return address >= state + sizeof(*bus) || address + bytes <= state;
    return !writable && span(address, bytes, FLASH_BASE, 1024U * 1024U);
}

/** \brief Confirm hardware and deadline between fixed-size settled prefixes. */
static nx_result_t words(nx_stm32f1_fsmc_t* bus, const uint16_t* tx,
                         uint16_t* rx, size_t count, size_t* transferred,
                         nx_time_us_t deadline) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    const void* data = tx != NULL ? (const void*)tx : (const void*)rx;
    if (count == 0U || count > 76800U ||
        !internal_storage(bus, data, count * sizeof(uint16_t), rx != NULL) ||
        !internal_storage(bus, transferred, sizeof(*transferred), true)) {
        return NX_ERROR_INVALID;
    }

    size_t done = 0U;
    const uintptr_t port = NX_STM32F1_FSMC_LCD_DATA;
    nx_result_t result = ready(bus, deadline);
    while (result == NX_SUCCESS && done < count) {
        const size_t end = count - done > 32U ? done + 32U : count;
        for (; done < end; ++done) {
            if (rx != NULL)
                rx[done] = NX_STM32F1_FSMC_READ16(port);
            else
                NX_STM32F1_FSMC_WRITE16(port, tx[done]);
        }
        nx_arch_dsb();
        result = ready(bus, deadline);
    }
    *transferred = done;
    return result;
}

static nx_result_t write_words(void* context, const uint16_t* data,
                               size_t count, size_t* transferred,
                               nx_time_us_t deadline) {
    return words(context, data, NULL, count, transferred, deadline);
}

static nx_result_t read_words(void* context, uint16_t* data, size_t count,
                              size_t* transferred, nx_time_us_t deadline) {
    return words(context, NULL, data, count, transferred, deadline);
}

const nx_parallel16_ops_t nx_stm32f1_fsmc_parallel16_ops = {
    command, write_words, read_words};

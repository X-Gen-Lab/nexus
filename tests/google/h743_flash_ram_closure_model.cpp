/**
 * \file            h743_flash_ram_closure_model.cpp
 * \brief           External ports, never an alternative Flash implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_ram_closure_support.h"
#include "nexus/core/fault.h"
#include <cstring>
#include <gtest/gtest.h>
namespace nexus_test::h743_ram {
RamCpuAndHardware cpu;
volatile uint32_t* Control() {
    return cpu.execution->bank == 0U ? &g_h743_model.flash.CR1
                                     : &g_h743_model.flash.CR2;
}
volatile uint32_t* Status() {
    return cpu.execution->bank == 0U ? &g_h743_model.flash.SR1
                                     : &g_h743_model.flash.SR2;
}
bool PrivateStack() {
    const uintptr_t first = reinterpret_cast<uintptr_t>(cpu.execution->stack);
    const uintptr_t end = first + sizeof(cpu.execution->stack);
    const uintptr_t active = (cpu.control & 2U) != 0U ? cpu.psp : cpu.msp;
    return (cpu.control & 2U) == 0U && first <= active && active < end;
}
void ObserveTrigger() {
    if (cpu.observer != nullptr) {
        cpu.observer->Trigger(cpu.primask == 1U,
                              g_h743_model.core->VTOR == ram_vector_address,
                              PrivateStack());
    }
}
} /* namespace nexus_test::h743_ram */
using namespace nexus_test::h743_ram;

extern "C" nx_stm32h743_flash_ram_workspace_t*
nx_h743_flash_ram_closure_current(void) {
    return g_h743_model.core->VTOR == ram_vector_address ? cpu.execution
                                                         : nullptr;
}
extern "C" uint32_t nx_h743_flash_ram_closure_control(void) {
    return cpu.control;
}
extern "C" uint32_t nx_h743_flash_ram_closure_ipsr(void) {
    return cpu.ipsr;
}
extern "C" uintptr_t nx_h743_flash_ram_closure_msp(void) {
    return cpu.msp;
}
extern "C" uintptr_t nx_h743_flash_ram_closure_psp(void) {
    return cpu.psp;
}
extern "C" uint32_t nx_h743_flash_ram_closure_basepri(void) {
    return cpu.basepri;
}
extern "C" uint32_t nx_h743_flash_ram_closure_faultmask(void) {
    return cpu.faultmask;
}
extern "C" uint32_t nx_h743_flash_ram_closure_fpccr(void) {
    return cpu.fpccr;
}
extern "C" uint32_t nx_h743_flash_ram_closure_mpu(void) {
    return cpu.mpu_control;
}
extern "C" uintptr_t nx_h743_flash_ram_closure_symbol(const char* name) {
    if (std::strcmp(name, "__nx_flash_ram_start") == 0)
        return cpu.code_start;
    if (std::strcmp(name, "__nx_flash_ram_end") == 0)
        return cpu.code_end;
    if (std::strcmp(name, "__nx_flash_ram_load_start") == 0)
        return cpu.code_load;
    if (std::strcmp(name, "__nx_flash_ram_load_end") == 0)
        return cpu.code_load_end;
    if (std::strcmp(name, "g_pfnVectors") == 0)
        return cpu.original_vtor;
    if (std::strcmp(name, "Default_Handler") == 0)
        return cpu.default_handler;
    if (std::strcmp(name, "nx_stm32h743_flash_ram_nmi") == 0)
        return cpu.code_start + 33U;
    if (std::strcmp(name, "nx_stm32h743_flash_ram_hardfault") == 0)
        return cpu.code_start + 65U;
    ADD_FAILURE() << "Unmapped real-link symbol requested: " << name;
    return 0U;
}
namespace {
bool InHostSpan(uintptr_t address, const void* object, size_t bytes) {
    const uintptr_t first = reinterpret_cast<uintptr_t>(object);
    return first <= address && address - first < bytes;
}
} /* namespace */
extern "C" uintptr_t nx_h743_flash_ram_closure_address(const void* object) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(object);
    /* One exact negative-fixture address mapping. This supplies target CPU
     * addresses for partial-edge host pointers without dereferencing them. */
    if (cpu.alias_pointer != 0U && address == cpu.alias_pointer) {
        return cpu.alias_address;
    }
    if (cpu.execution != nullptr) {
        const uintptr_t first = reinterpret_cast<uintptr_t>(cpu.execution);
        /* The before/after bytes exist only in the enclosing-object boundary
         * tests. Integer mapping does not dereference a fictional neighbor. */
        if (first >= 64U && first - 64U <= address &&
            address - (first - 64U) < sizeof(*cpu.execution) + 128U)
            return ram_vector_address - 64U + address - (first - 64U);
    }
    if (cpu.owner_port != nullptr &&
        InHostSpan(address, cpu.owner_port, sizeof(*cpu.owner_port)))
        return 0x24002000U + address -
               reinterpret_cast<uintptr_t>(cpu.owner_port);
    if (cpu.secondary_execution != nullptr &&
        InHostSpan(address, cpu.secondary_execution,
                   sizeof(*cpu.secondary_execution)))
        return 0x2400c000U + address -
               reinterpret_cast<uintptr_t>(cpu.secondary_execution);
    if (cpu.secondary_port != nullptr &&
        InHostSpan(address, cpu.secondary_port, sizeof(*cpu.secondary_port)))
        return 0x24003000U + address -
               reinterpret_cast<uintptr_t>(cpu.secondary_port);
    if (InHostSpan(address, &g_nx_stm32h743_system,
                   sizeof(g_nx_stm32h743_system)))
        return 0x24001000U + address -
               reinterpret_cast<uintptr_t>(&g_nx_stm32h743_system);
    if (InHostSpan(address, cpu.code_run.data(), cpu.code_run.size()))
        return cpu.code_start + address -
               reinterpret_cast<uintptr_t>(cpu.code_run.data());
    if (InHostSpan(address, cpu.original_vectors.data(),
                   sizeof(cpu.original_vectors)))
        return cpu.original_vtor + address -
               reinterpret_cast<uintptr_t>(cpu.original_vectors.data());
    if (InHostSpan(address, cpu.code_source.data(), cpu.code_source.size()))
        return cpu.code_load + address -
               reinterpret_cast<uintptr_t>(cpu.code_source.data());
    return address;
}
extern "C" uint8_t nx_h743_flash_ram_closure_read8(uintptr_t address) {
    if (cpu.code_load <= address &&
        address - cpu.code_load < cpu.code_source.size())
        return cpu.code_source[address - cpu.code_load];
    if (cpu.code_start <= address &&
        address - cpu.code_start < cpu.code_run.size())
        return cpu.code_run[address - cpu.code_start];
    ADD_FAILURE() << "Read outside the external code/load address map";
    return 0U;
}
extern "C" void nx_h743_flash_ram_closure_write8(uintptr_t address,
                                                 uint8_t value) {
    if (cpu.code_start <= address &&
        address - cpu.code_start < cpu.code_run.size()) {
        ++cpu.copy_writes;
        cpu.code_run[address - cpu.code_start] =
            cpu.corrupt_copy && address == cpu.code_start
                ? static_cast<uint8_t>(value ^ 1U)
                : value;
        return;
    }
    ADD_FAILURE() << "Write outside the external executable-RAM address map";
}
extern "C" uint32_t nx_h743_flash_ram_closure_read32(uintptr_t address) {
    if (cpu.original_vtor <= address &&
        address - cpu.original_vtor < sizeof(cpu.original_vectors) &&
        (address - cpu.original_vtor) % 4U == 0U)
        return cpu.original_vectors[(address - cpu.original_vtor) / 4U];
    ADD_FAILURE() << "Read outside the original vector table";
    return 0U;
}
extern "C" nx_result_t
nx_h743_flash_ram_closure_enter(nx_stm32h743_flash_ram_workspace_t* execution) {
    /* External CPU-port model for the C caller/transaction integration only.
     * The actual bridge's instruction ordering/stack restoration is separately
     * checked from real ARM ELFs, never inferred from this model. No Flash
     * operation, drain, vector restoration or failure handling is emulated. */
    ++cpu.bridge_entries;
    const uintptr_t msp = cpu.msp;
    const uintptr_t psp = cpu.psp;
    const uint32_t control = cpu.control;
    const uint32_t primask = cpu.primask;
    cpu.primask = 1U;
    cpu.msp = reinterpret_cast<uintptr_t>(execution->stack) +
              sizeof(execution->stack) - 64U;
    cpu.control &= ~UINT32_C(2);
    const nx_result_t result = nx_stm32h743_flash_ram_transaction(execution);
    /* Always model the actual return sequence. An unsafe transaction return
     * therefore fails the terminal/ownership tests rather than being repaired
     * by a helpful model. */
    cpu.msp = msp;
    cpu.psp = psp;
    cpu.control = control;
    cpu.primask = primask;
    return result;
}

extern "C" uint32_t nx_h743_flash_ram_model_primask(void) {
    return cpu.primask;
}
extern "C" uintptr_t nx_h743_flash_ram_model_sp(void) {
    return (cpu.control & 2U) != 0U ? cpu.psp : cpu.msp;
}
extern "C" uint32_t nx_h743_flash_ram_model_vtor(
    const nx_stm32h743_flash_ram_workspace_t* execution) {
    EXPECT_EQ(cpu.execution, execution);
    return ram_vector_address;
}
extern "C" void nx_h743_flash_ram_model_barrier(void) {
    /* CPU ordering port, not a replacement for the pulse implementation.
     * Actual ARM barriers and closure are checked in the linked ELF. */
}
extern "C" uint32_t nx_h743_flash_ram_model_read(volatile uint32_t* reg) {
    return NX_H743_FLASH_RAM_BASE_READ(reg);
}
namespace {
void ObserveExceptionBoundary(unsigned stage) {
    if (cpu.inject_exception == 0U || cpu.inject_stage != stage ||
        cpu.callback_seen)
        return;
    cpu.callback_seen = true;
    const bool busy =
        ((g_h743_model.flash.SR1 | g_h743_model.flash.SR2) & busy_flags) != 0U;
    if (g_h743_model.core->VTOR != ram_vector_address) {
        EXPECT_FALSE(busy) << "A busy bank would supply an exception vector";
        /* The old default handler does not return on silicon. This event is
         * only a pre/post-trigger fetch-safety observation; it never pretends
         * to execute that handler and resume. */
        return;
    }
    EXPECT_TRUE(PrivateStack());
    EXPECT_EQ(1U, cpu.primask);
    cpu.ipsr = cpu.inject_exception;
    if (cpu.inject_exception == 2U)
        nx_stm32h743_flash_ram_nmi();
    else
        nx_stm32h743_flash_ram_hardfault();
    ADD_FAILURE() << "The real RAM exception terminal entry returned";
}
} /* namespace */
extern "C" void nx_h743_flash_ram_model_write(volatile uint32_t* reg,
                                              uint32_t value) {
    if (reg == &g_h743_model.core->VTOR) {
        ++cpu.vtor_writes;
        if (value == ram_vector_address)
            ObserveExceptionBoundary(1U);
        else
            ObserveExceptionBoundary(7U);
        if ((value == ram_vector_address && cpu.refuse_vtor_publish) ||
            (value != ram_vector_address && cpu.refuse_vtor_restore)) {
            ++cpu.refused_writes;
            ++g_h743_model.writes;
            return;
        }
    }
    if (cpu.execution != nullptr && reg == Control()) {
        if ((value & FLASH_CR_START) != 0U) {
            ObserveTrigger();
            ObserveExceptionBoundary(3U);
        }
        if ((value & FLASH_CR_LOCK) != 0U)
            ObserveExceptionBoundary(6U);
        const bool refused_config =
            cpu.reject_configuration && value == cpu.execution->configuration;
        if (refused_config ||
            (cpu.reject_lock && (value & FLASH_CR_LOCK) != 0U)) {
            ++g_h743_model.writes;
            return;
        }
    }
    if (cpu.stuck_timer_ack && reg == &g_h743_model.timer.SR) {
        ++g_h743_model.writes;
        return;
    }
    NX_H743_FLASH_RAM_BASE_WRITE(reg, value);
    if (reg == &g_h743_model.core->VTOR)
        ObserveExceptionBoundary(value == ram_vector_address ? 2U : 8U);
    if (cpu.execution != nullptr && reg == Control() &&
        (value & FLASH_CR_START) != 0U && (*Control() & FLASH_CR_START) != 0U) {
        cpu.triggered = true;
        cpu.pulse_polls = 0U;
        *Status() = cpu.pending;
    }
}
extern "C" uint8_t nx_h743_flash_model_read8(const volatile uint8_t* address) {
    const nx_stm32h743_flash_state_t* port = cpu.owner_port;
    if (port == nullptr || port->memory == nullptr) {
        ADD_FAILURE() << "Flash data read without external memory backing";
        return 0U;
    }
    const uintptr_t first = reinterpret_cast<uintptr_t>(port->memory);
    const uintptr_t current = reinterpret_cast<uintptr_t>(address);
    if (current < first || current - first >= 2097152U) {
        ADD_FAILURE() << "Flash data read outside physical backing";
        return 0U;
    }
    EXPECT_EQ(0U,
              (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) & busy_flags);
    const uint8_t value = *address;
    const uintptr_t offset = current - first;
    if (cpu.inject_read_ecc && cpu.read_ecc_events == 0U &&
        offset == cpu.read_ecc_offset &&
        g_h743_model.flash_store_count >= cpu.read_ecc_after_stores) {
        volatile uint32_t* status = offset < 1048576U ? &g_h743_model.flash.SR1
                                                      : &g_h743_model.flash.SR2;
        /* RM0433 4.7.7: single-error correction returns corrected read data
         * while raising this bank's sticky SNECCERR. No owner is settled. */
        *status |= FLASH_SR_SNECCERR;
        ++cpu.read_ecc_events;
    }
    return value;
}
extern "C" void nx_h743_flash_ram_model_store(volatile uint32_t* address,
                                              uint32_t value) {
    if (g_h743_model.flash_store_count % 8U == 0U)
        cpu.pulse_polls = 0U;
    ObserveTrigger();
    if (g_h743_model.flash_store_count % 8U == 0U)
        ObserveExceptionBoundary(3U);
    nx_h743_model_flash_write32(address, value);
    cpu.triggered = true;
    *Status() = cpu.pending;
    if (g_h743_model.flash_store_count % 8U == 0U)
        ObserveExceptionBoundary(4U);
}
extern "C" void
nx_h743_flash_ram_model_poll(nx_stm32h743_flash_ram_workspace_t* execution) {
    EXPECT_EQ(cpu.execution, execution);
    ++cpu.polls;
    ++cpu.pulse_polls;
    g_h743_model.timer.CNT += g_h743_model.flash_poll_advance;
    if (cpu.overflow_on_first_poll && cpu.polls == 1U) {
        g_h743_model.timer.CNT = 4U;
        g_h743_model.timer.SR |= TIM_SR_UIF;
    }
    if (cpu.triggered && cpu.clear_after != 0U &&
        cpu.pulse_polls >= cpu.clear_after) {
        *Status() = static_cast<uint32_t>(cpu.pulse_error ? FLASH_SR_OPERR
                                                          : FLASH_SR_EOP);
    }
    if (cpu.observer != nullptr) {
        cpu.observer->Poll(*Status() & busy_flags, execution->active,
                           execution->port->active);
    }
    if ((*Status() & busy_flags) != 0U)
        ObserveExceptionBoundary(5U);
}
extern "C" void nx_h743_flash_ram_model_terminal_observe(
    nx_stm32h743_flash_ram_workspace_t* execution) {
    ASSERT_NE(nullptr, execution);
    if (cpu.observer != nullptr) {
        cpu.observer->Terminal(execution->fault);
    }
}

namespace nexus_test::h743_ram {
void ResetRamCpu(nx_stm32h743_flash_ram_workspace_t* execution) {
    cpu = {};
    cpu.execution = execution;
    cpu.owner_port = execution != nullptr ? execution->port : nullptr;
    cpu.primask = 0U;
    cpu.msp = execution != nullptr
                  ? reinterpret_cast<uintptr_t>(execution->stack) +
                        sizeof(execution->stack) - 64U
                  : uintptr_t{0x2407f000U};
    cpu.pending = busy_flags;
    cpu.clear_after = 3U;
    cpu.psp = uintptr_t{0x2407e000U};
    cpu.fpccr = UINT32_C(0xc0000000);
    cpu.code_start = 0x24008000U;
    cpu.code_end = cpu.code_start + 128U;
    cpu.code_load = 0x08004000U;
    cpu.code_load_end = cpu.code_load + 128U;
    cpu.original_vtor = 0x08000000U;
    cpu.default_handler = 0x08001001U;
    for (unsigned i = 0U; i < cpu.code_source.size(); ++i) {
        cpu.code_source[i] = static_cast<uint8_t>(i ^ 0x5aU);
    }
    cpu.original_vectors.fill(cpu.default_handler);
    cpu.original_vectors[0] = 0x24080000U;
    cpu.original_vectors[1] = 0x08000101U;
    NX_H743_FLASH_RAM_EXTERNAL_RESET(execution);
}
} /* namespace nexus_test::h743_ram */

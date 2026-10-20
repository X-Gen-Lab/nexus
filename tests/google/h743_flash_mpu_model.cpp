/**
 * \file            h743_flash_mpu_model.cpp
 * \brief           Banked hardware-register behavior, never a guard FSM
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_mpu_support.h"
#include <gtest/gtest.h>

#ifndef NX_H743_FLASH_MPU_BASE_READ
#define NX_H743_FLASH_MPU_BASE_READ(reg) nx_h743_model_read(reg)
#endif
#ifndef NX_H743_FLASH_MPU_BASE_WRITE
#define NX_H743_FLASH_MPU_BASE_WRITE(reg, value)                               \
    nx_h743_model_write((reg), (value))
#endif

namespace nexus_test::h743_mpu {
MpuHardware mpu;
void MpuHardware::Reset() {
    /* TYPE is a CMSIS read-only member. Never memset or write through it. */
    registers.CTRL = 0U;
    registers.RNR = 5U;
    registers.RBAR = 0U;
    registers.RASR = 0U;
    type = 16U << 8U;
    for (unsigned i = 0U; i < regions.size(); ++i) {
        regions[i] = {0x20000000U + i * 0x10000U, 0x03000000U | (i << 8U)};
    }
    region_reads.fill(0U);
    writes.clear();
    refusals.clear();
    triggers.clear();
    pc = h743_ram::cpu.code_start + 16U;
    dsbs = isbs = enable_dsb = enable_isb = 0U;
    sequence = base_write = base_read = attributes_write = 0U;
    attributes_read = control_write = control_read = last_dsb = last_isb = 0U;
    unsafe_triggers = ccr_writes = inject_exception = 0U;
    advance_time_on_arm = arm_time_advances = 0U;
    writes_at_last_trigger = 0U;
    inject_register = Register::Other;
    inject_value = 0U;
    inject_after = injected = false;
    guard = nullptr;
}
bool InsidePod() {
    return h743_ram::cpu.code_start <= mpu.pc &&
           mpu.pc < h743_ram::cpu.code_end;
}
bool GuardHasArchitecturalAttributes(bool handler) {
    const uint32_t control = mpu.registers.CTRL;
    return (control & 1U) != 0U && (!handler || (control & 2U) != 0U) &&
           mpu.regions[selector].base == 0U &&
           mpu.regions[selector].attributes == guard_attributes;
}
bool CodeInstructionPermitted(bool handler) {
    /* Narrow architectural oracle for this exact highest-region encoding.
     * This is not an M7 branch-predictor or arbitrary MPU emulator. */
    return !GuardHasArchitecturalAttributes(handler) ||
           (mpu.regions[selector].attributes & (1U << 28U)) == 0U;
}
bool CodeSpeculativeReadPermitted(bool handler) {
    if (!GuardHasArchitecturalAttributes(handler))
        return true;
    return (mpu.regions[selector].attributes & 0x003b0000U) != 0U;
}
bool ExplicitFlashWordStorePermitted(bool handler) {
    if (!GuardHasArchitecturalAttributes(handler))
        return false;
    return ((mpu.regions[selector].attributes >> 24U) & 7U) == 1U;
}
namespace {
Access Observation(Register reg, uint32_t value) {
    const uint32_t pending = (g_h743_model.flash.SR1 | g_h743_model.flash.SR2) &
                             h743_ram::busy_flags;
    const uint32_t control =
        h743_ram::cpu.execution != nullptr ? *h743_ram::Control() : 0U;
    return {reg,
            value,
            pending,
            control,
            mpu.pc,
            h743_ram::PrivateStack(),
            g_h743_model.core->VTOR};
}
Register Kind(const volatile uint32_t* reg) {
    if (reg == &mpu.registers.RNR)
        return Register::Selector;
    if (reg == &mpu.registers.RBAR)
        return Register::Base;
    if (reg == &mpu.registers.RASR)
        return Register::Attributes;
    if (reg == &mpu.registers.CTRL)
        return Register::Control;
    return Register::Other;
}
void MaybeException(Register reg, uint32_t value, bool after) {
    if (mpu.inject_exception == 0U || mpu.injected ||
        mpu.inject_register != reg || mpu.inject_value != value ||
        mpu.inject_after != after)
        return;
    mpu.injected = true;
    h743_ram::cpu.ipsr = mpu.inject_exception;
    if (mpu.inject_exception == 2U)
        nx_stm32h743_flash_ram_nmi();
    else
        nx_stm32h743_flash_ram_hardfault();
    ADD_FAILURE() << "RAM NMI/HardFault terminal unexpectedly returned";
}
} /* namespace */
void ObserveTrigger() {
    mpu.writes_at_last_trigger = mpu.writes.size();
    mpu.triggers.push_back(Observation(Register::Other, 0U));
    const bool read_back = mpu.base_read > mpu.base_write &&
                           mpu.attributes_read > mpu.attributes_write &&
                           mpu.control_read > mpu.control_write;
    const bool barriers =
        mpu.last_dsb > mpu.control_write && mpu.last_isb > mpu.last_dsb;
    if (!InsidePod() || !h743_ram::PrivateStack() ||
        g_h743_model.core->VTOR != h743_ram::ram_vector_address ||
        h743_ram::cpu.primask != 1U || !read_back || !barriers ||
        !GuardHasArchitecturalAttributes(false) ||
        CodeInstructionPermitted(false) ||
        CodeSpeculativeReadPermitted(false) ||
        !ExplicitFlashWordStorePermitted(false))
        ++mpu.unsafe_triggers;
}
} /* namespace nexus_test::h743_mpu */

using namespace nexus_test;

extern "C" MPU_Type* nx_h743_flash_mpu_registers(void) {
    return &h743_mpu::mpu.registers;
}
extern "C" uintptr_t nx_h743_flash_mpu_pc(void) {
    return h743_mpu::mpu.pc;
}
extern "C" uintptr_t nx_h743_flash_mpu_address(const void* object) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(object);
    const auto* execution = h743_ram::cpu.execution;
    if (execution != nullptr) {
        const uintptr_t first = reinterpret_cast<uintptr_t>(execution);
        if (first <= address && address - first < sizeof(*execution))
            return nx_h743_flash_ram_closure_address(object);
    }
    const uintptr_t first = reinterpret_cast<uintptr_t>(h743_mpu::mpu.guard);
    if (h743_mpu::mpu.guard != nullptr && first <= address &&
        address - first < sizeof(*h743_mpu::mpu.guard))
        return 0x24006000U + address - first;
    return nx_h743_flash_ram_closure_address(object);
}
extern "C" uint32_t nx_h743_flash_mpu_read(const volatile uint32_t* reg) {
    using namespace h743_mpu;
    ++mpu.sequence;
    if (reg == &mpu.registers.TYPE)
        return mpu.type;
    if (reg == &mpu.registers.CTRL) {
        mpu.control_read = mpu.sequence;
        return mpu.registers.CTRL;
    }
    if (reg == &mpu.registers.RNR)
        return mpu.registers.RNR;
    if (reg == &mpu.registers.RBAR || reg == &mpu.registers.RASR) {
        if (mpu.registers.RNR >= mpu.regions.size()) {
            ADD_FAILURE() << "Selected an unimplemented MPU descriptor";
            return 0U;
        }
        const auto& region = mpu.regions[mpu.registers.RNR];
        if (reg == &mpu.registers.RBAR) {
            if (mpu.registers.RNR == selector)
                mpu.base_read = mpu.sequence;
            /* PM0253 Table88: VALID reads zero; REGION reads current RNR. */
            return region.base | mpu.registers.RNR;
        }
        ++mpu.region_reads[mpu.registers.RNR];
        if (mpu.registers.RNR == selector)
            mpu.attributes_read = mpu.sequence;
        return region.attributes;
    }
    return NX_H743_FLASH_MPU_BASE_READ(const_cast<volatile uint32_t*>(reg));
}
extern "C" void nx_h743_flash_mpu_write(volatile uint32_t* reg,
                                        uint32_t value) {
    using namespace h743_mpu;
    const Register kind = Kind(reg);
    if (kind != Register::Other) {
        ++mpu.sequence;
        mpu.writes.push_back(Observation(kind, value));
        if (kind != Register::Selector) {
            EXPECT_TRUE(InsidePod());
            EXPECT_TRUE(h743_ram::PrivateStack());
            EXPECT_EQ(h743_ram::ram_vector_address, g_h743_model.core->VTOR);
            EXPECT_EQ(1U, h743_ram::cpu.primask);
        }
        MaybeException(kind, value, false);
        for (const auto& refusal : mpu.refusals) {
            if (refusal.reg == kind &&
                (refusal.value & refusal.mask) == (value & refusal.mask) &&
                (!refusal.after_trigger || h743_ram::cpu.triggered))
                return;
        }
        if (kind == Register::Selector) {
            mpu.registers.RNR = value;
        } else if (kind == Register::Control) {
            mpu.registers.CTRL = value;
            h743_ram::cpu.mpu_control = value;
            mpu.control_write = mpu.sequence;
            if (value == guard_control) {
                mpu.enable_dsb = mpu.dsbs;
                mpu.enable_isb = mpu.isbs;
            }
        } else {
            if (mpu.registers.RNR >= mpu.regions.size()) {
                ADD_FAILURE() << "Write to an unimplemented MPU descriptor";
                return;
            }
            auto& region = mpu.regions[mpu.registers.RNR];
            if (kind == Register::Base) {
                /* All allowed guard/saved values use RBAR.VALID=0. No model
                 * shortcut converts an unreviewed VALID-region update. */
                EXPECT_EQ(0U, value & 0x10U);
                region.base = value & ~UINT32_C(0x1f);
                if (mpu.registers.RNR == selector)
                    mpu.base_write = mpu.sequence;
            } else {
                region.attributes = value;
                if (mpu.registers.RNR == selector)
                    mpu.attributes_write = mpu.sequence;
            }
        }
        MaybeException(kind, value, true);
        return;
    }
    if (reg == &g_h743_model.core->CCR)
        ++mpu.ccr_writes;
    if ((reg == &g_h743_model.flash.CR1 || reg == &g_h743_model.flash.CR2) &&
        (value & static_cast<uint32_t>(FLASH_CR_START)) != 0U)
        h743_mpu::ObserveTrigger();
    NX_H743_FLASH_MPU_BASE_WRITE(reg, value);
    const auto* execution = h743_ram::cpu.execution;
    if (execution != nullptr && reg == h743_ram::Control() &&
        value == execution->configuration &&
        (value & static_cast<uint32_t>(FLASH_CR_PG | FLASH_CR_SER)) != 0U &&
        mpu.advance_time_on_arm != 0U && mpu.arm_time_advances == 0U) {
        const uint32_t previous = g_h743_model.timer.CNT;
        g_h743_model.timer.CNT += mpu.advance_time_on_arm;
        if (g_h743_model.timer.CNT < previous)
            g_h743_model.timer.SR |= TIM_SR_UIF;
        ++mpu.arm_time_advances;
    }
}
extern "C" void nx_h743_flash_mpu_dsb(void) {
    using namespace h743_mpu;
    ++mpu.dsbs;
    mpu.last_dsb = ++mpu.sequence;
    mpu.writes.push_back(Observation(Register::Dsb, 0U));
}
extern "C" void nx_h743_flash_mpu_isb(void) {
    using namespace h743_mpu;
    ++mpu.isbs;
    mpu.last_isb = ++mpu.sequence;
    mpu.writes.push_back(Observation(Register::Isb, 0U));
}
extern "C" void
nx_h743_flash_mpu_poll(nx_stm32h743_flash_ram_workspace_t* execution) {
    nx_h743_flash_ram_model_poll(execution);
    h743_mpu::MaybeException(h743_mpu::Register::Other, h743_ram::cpu.polls,
                             true);
}
extern "C" void nx_h743_flash_mpu_store(volatile uint32_t* address,
                                        uint32_t value) {
    h743_mpu::ObserveTrigger();
    nx_h743_flash_ram_model_store(address, value);
}
extern "C" void
nx_h743_flash_mpu_terminal(nx_stm32h743_flash_ram_workspace_t* execution,
                           nx_stm32h743_flash_mpu_state_t* guard) {
    EXPECT_EQ(h743_ram::cpu.execution, execution);
    EXPECT_EQ(h743_mpu::mpu.guard != nullptr ? h743_mpu::mpu.guard
                                             : &execution->guard,
              guard);
    if (h743_ram::cpu.observer != nullptr)
        h743_ram::cpu.observer->Terminal(
            guard->fault == NX_H743_FLASH_MPU_FAULT_LOCK
                ? NX_H743_FLASH_RAM_FAULT_LOCK
                : NX_H743_FLASH_RAM_FAULT_EXCEPTION);
}

extern "C" void
nx_h743_flash_mpu_model_reset(nx_stm32h743_flash_ram_workspace_t* execution) {
    h743_mpu::mpu.Reset();
    h743_mpu::mpu.guard = execution != nullptr ? &execution->guard : nullptr;
}

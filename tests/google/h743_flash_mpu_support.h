/**
 * \file            h743_flash_mpu_support.h
 * \brief           External register traces and narrow architecture oracle
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_MPU_SUPPORT_H
#define NX_H743_FLASH_MPU_SUPPORT_H
#include "h743_flash_mpu_model.h"
#include "h743_flash_ram_closure_support.h"
#include <array>
#include <vector>
namespace nexus_test::h743_mpu {
constexpr uint32_t guard_attributes = UINT32_C(0x11000039);
constexpr uint32_t guard_control = 7U;
constexpr uint32_t selector = 15U;
enum class Register { Other, Selector, Base, Attributes, Control, Dsb, Isb };
struct Region {
    uint32_t base;
    uint32_t attributes;
    bool operator==(const Region& other) const {
        return base == other.base && attributes == other.attributes;
    }
};
struct Access {
    Register reg;
    uint32_t value;
    uint32_t pending;
    uint32_t flash_control;
    uintptr_t pc;
    bool private_stack;
    uint32_t vtor;
};
struct Refusal {
    Register reg;
    uint32_t value;
    bool after_trigger;
    uint32_t mask = UINT32_MAX;
};
struct MpuHardware {
    MPU_Type registers{};
    uint32_t type;
    std::array<Region, 16U> regions;
    std::array<unsigned, 16U> region_reads;
    std::vector<Access> writes;
    std::vector<Refusal> refusals;
    std::vector<Access> triggers;
    uintptr_t pc;
    unsigned dsbs;
    unsigned isbs;
    unsigned enable_dsb;
    unsigned enable_isb;
    uint64_t sequence;
    uint64_t base_write;
    uint64_t base_read;
    uint64_t attributes_write;
    uint64_t attributes_read;
    uint64_t control_write;
    uint64_t control_read;
    uint64_t last_dsb;
    uint64_t last_isb;
    unsigned unsafe_triggers;
    unsigned ccr_writes;
    uint32_t advance_time_on_arm;
    unsigned arm_time_advances;
    size_t writes_at_last_trigger;
    unsigned inject_exception;
    Register inject_register;
    uint32_t inject_value;
    bool inject_after;
    bool injected;
    nx_stm32h743_flash_mpu_state_t* guard;
    void Reset();
};
extern MpuHardware mpu;
bool InsidePod();
bool GuardHasArchitecturalAttributes(bool handler);
bool CodeInstructionPermitted(bool handler);
bool CodeSpeculativeReadPermitted(bool handler);
bool ExplicitFlashWordStorePermitted(bool handler);
void ObserveTrigger();
} /* namespace nexus_test::h743_mpu */
#endif

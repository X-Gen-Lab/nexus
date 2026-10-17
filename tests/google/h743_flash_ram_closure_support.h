/**
 * \file            h743_flash_ram_closure_support.h
 * \brief           External CPU, address-map and MMIO test observations
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_RAM_CLOSURE_SUPPORT_H
#define NX_H743_FLASH_RAM_CLOSURE_SUPPORT_H
#include "h743_flash_ram_closure_model.h"
#include <array>
#include <gmock/gmock.h>
namespace nexus_test::h743_ram {
constexpr uint32_t busy_flags =
    static_cast<uint32_t>(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE);
constexpr uint32_t ram_vector_address = 0x24004000U;
class RamHardwareBoundary {
  public:
    MOCK_METHOD(void, Trigger,
                (bool masked, bool private_vector, bool private_stack));
    MOCK_METHOD(void, Poll,
                (uint32_t pending, bool execution_retained,
                 bool controller_retained));
    MOCK_METHOD(void, Terminal, (nx_stm32h743_flash_ram_fault_t reason));
};
struct RamCpuAndHardware {
    nx_stm32h743_flash_ram_workspace_t* execution;
    nx_stm32h743_flash_state_t* owner_port;
    nx_stm32h743_flash_ram_workspace_t* secondary_execution;
    nx_stm32h743_flash_state_t* secondary_port;
    RamHardwareBoundary* observer;
    uint32_t primask;
    uintptr_t msp;
    uintptr_t psp;
    uint32_t control;
    uint32_t ipsr;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t fpccr;
    uint32_t mpu_control;
    uint32_t code_start;
    uint32_t code_end;
    uint32_t code_load;
    uint32_t code_load_end;
    uint32_t original_vtor;
    uint32_t default_handler;
    unsigned copy_writes;
    unsigned vector_writes;
    unsigned vtor_writes;
    unsigned bridge_entries;
    uintptr_t alias_pointer;
    uintptr_t alias_address;
    unsigned refused_writes;
    bool corrupt_copy;
    bool refuse_vtor_publish;
    bool refuse_vtor_restore;
    bool callback_seen;
    unsigned inject_exception;
    unsigned inject_stage;
    std::array<uint8_t, 128U> code_source;
    std::array<uint8_t, 128U> code_run;
    std::array<uint32_t, NX_H743_FLASH_RAM_VECTOR_WORDS> original_vectors;
    uint32_t pending;
    unsigned polls;
    unsigned pulse_polls;
    unsigned clear_after;
    unsigned flash_fault_observers;
    bool reject_configuration;
    bool reject_lock;
    bool pulse_error;
    bool overflow_on_first_poll;
    bool stuck_timer_ack;
    bool triggered;
    bool inject_read_ecc;
    uint32_t read_ecc_offset;
    uint32_t read_ecc_after_stores;
    unsigned read_ecc_events;
};
extern RamCpuAndHardware cpu;
volatile uint32_t* Control();
volatile uint32_t* Status();
bool PrivateStack();
void ObserveTrigger();
void ResetRamCpu(nx_stm32h743_flash_ram_workspace_t* execution);
} /* namespace nexus_test::h743_ram */
#endif

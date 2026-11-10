/**
 * \file            i2c_target_exchange_model.cpp
 * \brief           Real SR1/SR2, DR, reset, NVIC and clock boundary semantics
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_target_exchange_model.h"
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_i2c_target.h"
}
#include <cstring>

extern "C" {
RCC_TypeDef nx_target_rcc;
I2C_TypeDef nx_target_i2c[3];
GPIO_TypeDef nx_target_gpio[9];
nx_stm32_system_t g_nx_stm32_system;
}

namespace nx_i2c_target_model {
Model hardware;

static void crc_byte(unsigned instance, uint8_t value) {
    if ((nx_target_i2c[instance].CR1 & I2C_CR1_ENPEC) == 0U)
        return;
    uint8_t& crc = hardware.crc[instance];
    crc ^= value;
    for (unsigned bit = 0U; bit < 8U; ++bit)
        crc = (uint8_t)((crc & 0x80U) != 0U ? (crc << 1U) ^ 7U : crc << 1U);
    nx_target_i2c[instance].SR2 =
        (nx_target_i2c[instance].SR2 & ~UINT32_C(0xff00)) |
        ((uint32_t)crc << 8U);
}

void reset() {
    hardware = {};
    hardware.priority.fill(3U);
    nx_target_rcc = {};
    std::memset(nx_target_i2c, 0, sizeof(nx_target_i2c));
    std::memset(nx_target_gpio, 0, sizeof(nx_target_gpio));
}

void address(unsigned instance, bool transmitter, bool second) {
    auto& peripheral = nx_target_i2c[instance];
    peripheral.CR1 &= ~(uint32_t)I2C_CR1_PEC;
    peripheral.SR2 = I2C_SR2_BUSY | (transmitter ? I2C_SR2_TRA : 0U) |
                     (second ? I2C_SR2_DUALF : 0U);
    uint32_t selected = second ? peripheral.OAR2 : peripheral.OAR1;
    crc_byte(instance, (uint8_t)((selected & 0xfeU) | (transmitter ? 1U : 0U)));
    peripheral.SR1 |= I2C_SR1_ADDR;
    if (transmitter)
        peripheral.SR1 |= I2C_SR1_TXE;
}

void bytes(unsigned instance, std::initializer_list<uint8_t> values,
           bool stop) {
    auto& queue = hardware.receive[instance];
    for (uint8_t value : values) {
        queue.push_back(value);
        crc_byte(instance, value);
    }
    auto& peripheral = nx_target_i2c[instance];
    if (!queue.empty()) {
        peripheral.DR = queue.front();
        peripheral.SR1 |= I2C_SR1_RXNE;
    }
    if (queue.size() >= 2U)
        peripheral.SR1 |= I2C_SR1_BTF;
    if (stop) {
        peripheral.SR1 |= I2C_SR1_STOPF;
        peripheral.SR2 &=
            ~(uint32_t)(I2C_SR2_BUSY | I2C_SR2_TRA | I2C_SR2_DUALF);
        peripheral.CR1 &= ~(uint32_t)I2C_CR1_PEC;
    }
}

void flag(unsigned instance, uint32_t sr1) {
    nx_target_i2c[instance].SR1 |= sr1;
    if ((sr1 & I2C_SR1_SMBALERT) != 0U) {
        nx_target_i2c[instance].CR1 &= ~(uint32_t)I2C_CR1_PEC;
        nx_target_i2c[instance].SR2 |= I2C_SR2_BUSY | I2C_SR2_TRA;
        crc_byte(instance, 0x19U);
    }
    if ((sr1 & I2C_SR1_ARLO) != 0U) {
        nx_target_i2c[instance].SR2 &= ~(uint32_t)(I2C_SR2_TRA | I2C_SR2_MSL);
        /* RM0090 marks the accumulator corrupt after ARLO. This explicit
         * external sample is not a claim about the physical corrupt value. */
        hardware.crc[instance] = 0x5AU;
    }
    if ((sr1 & I2C_SR1_AF) != 0U)
        nx_target_i2c[instance].CR1 &= ~(uint32_t)I2C_CR1_PEC;
}

void interrupt(unsigned instance, void* owner) {
    const IRQn_Type events[] = {I2C1_EV_IRQn, I2C2_EV_IRQn, I2C3_EV_IRQn};
    size_t index = static_cast<size_t>(events[instance]);
    hardware.in_isr = true;
    hardware.active[index] = 1U;
    nx_stm32_i2c_target_irq(static_cast<nx_stm32_i2c_target_state_t*>(owner));
    hardware.active[index] = 0U;
    hardware.in_isr = false;
}

uint8_t crc8(std::initializer_list<uint8_t> values) {
    uint8_t crc = 0U;
    for (uint8_t byte : values) {
        crc ^= byte;
        for (unsigned bit = 0U; bit < 8U; ++bit)
            crc = (uint8_t)((crc & 0x80U) != 0U ? (crc << 1U) ^ 7U : crc << 1U);
    }
    return crc;
}
} /* namespace nx_i2c_target_model */

/** \brief ADDR clears only after SR1/SR2; DR reads consume actual RX facts. */
extern "C" uint32_t nx_target_read(volatile uint32_t* address) {
    using namespace nx_i2c_target_model;
    uint32_t value = *address;
    for (unsigned instance = 0U; instance < 3U; ++instance) {
        auto& peripheral = nx_target_i2c[instance];
        if (address == &peripheral.SR1) {
            hardware.sr1_read[instance] = true;
            hardware.addr_seen[instance] = (value & I2C_SR1_ADDR) != 0U;
            hardware.stop_seen[instance] = (value & I2C_SR1_STOPF) != 0U;
        } else if (address == &peripheral.SR2) {
            if ((peripheral.CR1 & I2C_CR1_PE) != 0U &&
                !hardware.addr_seen[instance])
                ++hardware.unsafe_sr2_reads;
            if (hardware.sr1_read[instance] &&
                (peripheral.SR1 & I2C_SR1_ADDR) != 0U) {
                if (!hardware.addr_seen[instance])
                    ++hardware.accidental_addr_clears;
                peripheral.SR1 &= ~(uint32_t)I2C_SR1_ADDR;
            }
            hardware.sr1_read[instance] = false;
            hardware.addr_seen[instance] = false;
        } else if (address == &peripheral.DR) {
            auto& queue = hardware.receive[instance];
            if (!queue.empty()) {
                value = queue.front();
                queue.pop_front();
            }
            peripheral.SR1 &= ~(uint32_t)(I2C_SR1_RXNE | I2C_SR1_BTF);
            if (!queue.empty()) {
                peripheral.DR = queue.front();
                peripheral.SR1 |= I2C_SR1_RXNE;
            }
            if (queue.size() >= 2U)
                peripheral.SR1 |= I2C_SR1_BTF;
        }
    }
    hardware.accesses.push_back({address, value, false});
    if (hardware.after_read)
        hardware.after_read(address, value);
    return value;
}

/** \brief SR1 error bits are W0C; reset, STOPF and PEC have hardware effects.
 */
extern "C" void nx_target_write(volatile uint32_t* address, uint32_t value) {
    using namespace nx_i2c_target_model;
    ++hardware.writes;
    hardware.accesses.push_back({address, value, true});
    if (hardware.before_write)
        hardware.before_write(address, value);
    if (address == hardware.refused_write)
        value = (value & ~hardware.refused_bits) |
                (*address & hardware.refused_bits);
    if (address == &RCC->APB1RSTR && hardware.refuse_reset)
        return;
    for (unsigned instance = 0U; instance < 3U; ++instance) {
        auto& peripheral = nx_target_i2c[instance];
        if (address == &peripheral.SR1) {
            constexpr uint32_t errors =
                I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR |
                I2C_SR1_PECERR | I2C_SR1_TIMEOUT | I2C_SR1_SMBALERT;
            *address &= value | ~errors;
            return;
        }
        if (address == &peripheral.CR1) {
            if ((value & I2C_CR1_PEC) != 0U &&
                (peripheral.CR1 & I2C_CR1_PEC) == 0U) {
                ++hardware.pec_writes[instance];
                hardware.pec_values[instance].push_back(hardware.crc[instance]);
            }
            if ((value & I2C_CR1_PE) == 0U) {
                value &= ~(uint32_t)(I2C_CR1_ACK | I2C_CR1_POS | I2C_CR1_START |
                                     I2C_CR1_ALERT | I2C_CR1_PEC);
                hardware.crc[instance] = 0U;
                peripheral.SR2 &= I2C_SR2_BUSY;
            }
            if (hardware.stop_seen[instance]) {
                peripheral.SR1 &= ~(uint32_t)I2C_SR1_STOPF;
                hardware.stop_seen[instance] = false;
            }
        } else if (address == &peripheral.DR) {
            hardware.loaded[instance].push_back((uint8_t)value);
            crc_byte(instance, (uint8_t)value);
            peripheral.SR1 &= ~(uint32_t)(I2C_SR1_TXE | I2C_SR1_BTF);
        }
    }
    *address = value;
    if (address == &RCC->APB1RSTR) {
        for (unsigned instance = 0U; instance < 3U; ++instance) {
            if ((value & (RCC_APB1RSTR_I2C1RST << instance)) != 0U) {
                std::memset(&nx_target_i2c[instance], 0, sizeof(I2C_TypeDef));
                nx_target_i2c[instance].TRISE = 2U;
                hardware.receive[instance].clear();
                hardware.sr1_read[instance] = false;
                hardware.addr_seen[instance] = false;
                hardware.stop_seen[instance] = false;
                hardware.crc[instance] = 0U;
            }
        }
    }
}

/** \brief NVIC command refusal never substitutes a successful withdrawal. */
extern "C" uint32_t nx_target_irq(unsigned command, IRQn_Type irq,
                                  uint32_t value) {
    using namespace nx_i2c_target_model;
    size_t index = static_cast<size_t>(irq);
    if (index >= hardware.enabled.size())
        return 0U;
    switch (command) {
        case 0U:
            hardware.enabled[index] = 1U;
            break;
        case 1U:
            if (!hardware.refuse_disable)
                hardware.enabled[index] = 0U;
            break;
        case 2U:
            if (!hardware.refuse_clear)
                hardware.pending[index] = 0U;
            break;
        case 3U:
            hardware.priority[index] = value;
            break;
        case 4U:
            return hardware.enabled[index];
        case 5U:
            return hardware.pending[index];
        case 6U:
            return hardware.active[index];
        case 7U:
            return hardware.priority[index];
        default:
            break;
    }
    return 0U;
}

/** \brief Time is an externally controlled monotonic-domain observation. */
extern "C" uint64_t nx_stm32_system_now(nx_stm32_system_t*) {
    using namespace nx_i2c_target_model;
    uint64_t value = hardware.now;
    hardware.now += hardware.clock_step;
    ++hardware.clock_reads;
    if (hardware.after_clock)
        hardware.after_clock();
    return value;
}
extern "C" bool nx_target_privileged(void) {
    return nx_i2c_target_model::hardware.privileged;
}
extern "C" bool nx_target_in_isr(void) {
    return nx_i2c_target_model::hardware.in_isr;
}
extern "C" bool nx_target_masked(void) {
    return nx_i2c_target_model::hardware.masked;
}
extern "C" void nx_target_dsb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    auto& cpu = nx_i2c_target_model::hardware;
    nx_arch_irq_state_t saved{cpu.masked ? 1U : 0U};
    cpu.masked = true;
    return saved;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    nx_i2c_target_model::hardware.masked = saved.value != 0U;
}

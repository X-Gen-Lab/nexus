/**
 * \file            i2c_target_exchange_model.h
 * \brief           External peer, MMIO and CPU facts for finite target tests
 * \author          Nexus Team
 */
#ifndef NX_I2C_TARGET_EXCHANGE_MODEL_H
#define NX_I2C_TARGET_EXCHANGE_MODEL_H
#include "i2c_target_model.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace nx_i2c_target_model {
/** \brief Captured bus accesses let tests verify hardware clearing order. */
struct Access {
    volatile uint32_t* address;
    uint32_t value;
    bool write;
};
/** \brief External hardware controls, separate from provider policy/state. */
struct Model {
    uint64_t now = 10U;
    uint64_t clock_step = 0U;
    unsigned clock_reads = 0U;
    unsigned writes = 0U;
    unsigned unsafe_sr2_reads = 0U;
    unsigned accidental_addr_clears = 0U;
    bool privileged = true;
    bool masked = false;
    bool in_isr = false;
    bool refuse_disable = false;
    bool refuse_clear = false;
    bool refuse_reset = false;
    volatile uint32_t* refused_write = nullptr;
    uint32_t refused_bits = 0U;
    std::array<uint32_t, 82> enabled{};
    std::array<uint32_t, 82> pending{};
    std::array<uint32_t, 82> active{};
    std::array<uint32_t, 82> priority{};
    std::array<bool, 3> sr1_read{};
    std::array<bool, 3> addr_seen{};
    std::array<bool, 3> stop_seen{};
    std::array<unsigned, 3> pec_writes{};
    std::array<uint8_t, 3> crc{};
    std::array<std::vector<uint8_t>, 3> pec_values{};
    std::array<std::deque<uint8_t>, 3> receive{};
    std::array<std::vector<uint8_t>, 3> loaded{};
    std::vector<Access> accesses;
    std::function<void(volatile uint32_t*, uint32_t)> after_read;
    std::function<void(volatile uint32_t*, uint32_t)> before_write;
    std::function<void()> after_clock;
};
extern Model hardware;
void reset();
void address(unsigned instance, bool transmitter, bool second = false);
void bytes(unsigned instance, std::initializer_list<uint8_t> values,
           bool stop = false);
void flag(unsigned instance, uint32_t sr1);
void interrupt(unsigned instance, void* owner);
uint8_t crc8(std::initializer_list<uint8_t> bytes);
} /* namespace nx_i2c_target_model */
#endif /* NX_I2C_TARGET_EXCHANGE_MODEL_H */

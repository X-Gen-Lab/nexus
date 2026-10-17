/**
 * \file            ethernet_raw_errata_test.cpp
 * \brief           Proposed raw MAC ES0182 external-latch regression inputs
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "eth_mac.h"
#if defined(NX_ETH_TEST_STM32)
#include "stm32f407_eth.h"
#elif defined(NX_ETH_TEST_GD32)
extern "C" {
#include "gd32f470_eth.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
NVIC_Type g_gd32_model_nvic;
}
#endif
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
#include <sys/mman.h>
#endif
#include <array>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
namespace {
constexpr uint32_t own = UINT32_C(0x80000000);
constexpr unsigned ctl = 0x1018, dma_status = 0x1014;
uint64_t now;
uint64_t elapsed_ns;
uint64_t last_mac_write_ns;
uint32_t internally_latched_mac;
bool enforce_spacing;
bool illegal_flush;
bool hold_reset, hold_engine, hold_flush, hold_own, hold_mdio;
bool hold_table_clear;
uint16_t phy[32];
unsigned mdio_writes;
class EthernetMac : public testing::Test {
  protected:
    uint32_t regstore[0x1060 / 4]{};
    uint32_t* regs = regstore;
    void* registers_mapping = nullptr;
    void* memory_mapping = nullptr;
    nx_eth_descriptor_t txstore[2]{}, rxstore[2]{};
    nx_eth_descriptor_t* tx = txstore;
    nx_eth_descriptor_t* rx = rxstore;
    nx_eth_tx_slot_t txslots[2]{};
    nx_eth_rx_slot_t rxslots[2]{};
    alignas(4) uint8_t bufferstore[2][1520]{};
    uint8_t (*buffers)[1520] = bufferstore;
    alignas(4) uint8_t outputstore[64]{};
    uint8_t* output = outputstore;
    nx_dma_memory_region_t regions[1]{{1U, UINTPTR_MAX - 1U, 3U}};
    nx_eth_mac_t mac{};
    nx_eth_tx_request_t request{};
    const nx_eth_port_t port{&nx_eth_mac_ops, &mac};
    const nx_mdio_port_t mdio{&nx_eth_mdio_ops, &mac};
    void SetUp() override {
        now = 100;
        elapsed_ns = 1U;
        last_mac_write_ns = 0U;
        internally_latched_mac = 0U;
        enforce_spacing = false;
        illegal_flush = false;
        hold_reset = hold_engine = hold_flush = hold_own = hold_mdio = false;
        hold_table_clear = false;
        mdio_writes = 0;
        std::memset(phy, 0, sizeof(phy));
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
        registers_mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x40000000)),
                  registers_mapping);
        memory_mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x20000000)), 0x20000,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(UINT32_C(0x20000000)),
                  memory_mapping);
        regs = reinterpret_cast<uint32_t*>(UINT32_C(0x40028000));
        tx = reinterpret_cast<nx_eth_descriptor_t*>(UINT32_C(0x20000000));
        rx = reinterpret_cast<nx_eth_descriptor_t*>(UINT32_C(0x20000040));
        buffers = reinterpret_cast<uint8_t(*)[1520]>(UINT32_C(0x20001000));
        output = reinterpret_cast<uint8_t*>(UINT32_C(0x20003000));
        regions[0] = {UINT32_C(0x20000000), 0x20000, 3};
#if defined(NX_ETH_TEST_STM32)
        RCC->AHB1ENR |= UINT32_C(0x0e000000);
        SYSCFG->PMC |= UINT32_C(0x00800000);
#else
        RCU_AHB1EN |= UINT32_C(0x0e000000);
        SYSCFG_CFG1 |= UINT32_C(0x00800000);
#endif
#endif
        mac.registers = reinterpret_cast<uintptr_t>(regs);
        mac.regions = regions;
        mac.region_count = 1;
        mac.tx_descriptors = tx;
        mac.rx_descriptors = rx;
        mac.tx_slots = txslots;
        mac.rx_slots = rxslots;
        mac.tx_count = mac.rx_count = 2;
        mac.drain_timeout_us = 1000;
#if defined(NX_ETH_TEST_GD32)
        mac.hclk_hz = 200000000;
#else
        mac.hclk_hz = 168000000;
#endif
        mac.address[0] = 2;
        mac.address[5] = 1;
#if defined(NX_ETH_TEST_GD32)
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 91, 4, 0};
#else
        mac.irq_policy = {NX_IRQ_KERNEL_NONE, 82, 4, 0};
#endif
        mac.source = {61, 5, 0};
        rxslots[0].data = buffers[0];
        rxslots[1].data = buffers[1];
        rxslots[0].capacity = rxslots[1].capacity = 1520;
        output[12] = 0x08;
        nx_request_initialize(&request.base);
    }
    nx_result_t Initialize(nx_time_us_t deadline = 10000) {
#if defined(NX_ETH_TEST_STM32)
        return nx_stm32_eth_initialize(&mac, deadline);
#elif defined(NX_ETH_TEST_GD32)
        return nx_gd32_eth_initialize(&mac, deadline);
#else
        return nx_eth_mac_initialize(&mac, deadline);
#endif
    }
    void TearDown() override {
        /* Release injected external faults while request/metadata still live.
         */
        enforce_spacing = false;
        hold_engine = hold_reset = hold_flush = hold_own = hold_mdio = false;
        regs[0x34U / 4U] = 0U;
        if (mac.retained) {
            if (txslots[0].request != nullptr) {
                /* Explicit hardware completion for cleanup, never a provider
                 * helper that silently revokes the original payload borrow. */
                tx[0].status = UINT32_C(0x30000000);
            }
            nx_result_t result = NX_ERROR_BUSY;
            for (unsigned i = 0U; i < 8U && result != NX_SUCCESS; ++i) {
                nx_eth_model_poll(&mac);
                result = nx_eth_port_stop(&port);
            }
            EXPECT_EQ(NX_SUCCESS, result);
            EXPECT_FALSE(mac.retained);
            EXPECT_EQ(nullptr, txslots[0].request);
            if (mac.retained || txslots[0].request != nullptr ||
                txslots[1].request != nullptr || mac.wake_publishers != 0U) {
                ADD_FAILURE() << "Cannot unmap a retained raw MAC lease";
                std::abort();
            }
        }
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
        if (memory_mapping != nullptr && memory_mapping != MAP_FAILED)
            munmap(memory_mapping, 0x20000);
        if (registers_mapping != nullptr && registers_mapping != MAP_FAILED)
            munmap(registers_mapping, 0x80000);
#endif
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, Initialize());
        nx_eth_link_t link{true, true, 100};
        ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &link));
        ASSERT_EQ(NX_SUCCESS, nx_eth_tx_prepare(&request, output, 64, 10000));
    }
    void Receive(unsigned index, unsigned length = 68, uint32_t flags = 0x300) {
        std::memset(buffers[index], 0x22, 64);
        buffers[index][12] = 8;
        buffers[index][13] = 0;
        rx[index].status = (length << 16U) | flags;
        regs[dma_status / 4] |= 0x40;
        nx_eth_mac_irq(&mac);
    }
};
} /* namespace */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint32_t nx_eth_model_read(uintptr_t base, unsigned offset) {
    /* Model the actual HCLK read cost, not four TX/RX_CLK periods. */
#if defined(NX_ETH_TEST_GD32)
    elapsed_ns += 5U;
#else
    elapsed_ns += 6U;
#endif
    return reinterpret_cast<uint32_t*>(base)[offset / 4];
}
extern "C" void nx_eth_model_write(uintptr_t base, unsigned offset,
                                   uint32_t value) {
    auto* regs = reinterpret_cast<uint32_t*>(base);
#if defined(NX_ETH_TEST_GD32)
    elapsed_ns += 5U;
#else
    elapsed_ns += 6U;
#endif
    if (offset == ctl && (value & UINT32_C(0x00100000)) != 0U &&
        (regs[0x34U / 4U] & UINT32_C(0x01000000)) != 0U) {
        illegal_flush = true;
    }
    if (offset == 0U) {
        if (!enforce_spacing || last_mac_write_ns == 0U ||
            elapsed_ns - last_mac_write_ns >= 80U) {
            internally_latched_mac = value;
        }
        last_mac_write_ns = elapsed_ns;
    }
    if (hold_table_clear && (offset == 0x100c || offset == 0x1010) &&
        value == 0U)
        return;
    if (offset == dma_status) {
        regs[offset / 4] &= ~value;
        return;
    }
    if (offset == ctl && hold_engine) {
        value |= regs[offset / 4] & 0x2002;
    }
    regs[offset / 4] = value;
}
extern "C" void nx_eth_model_descriptor(void* descriptor, uint32_t value) {
    auto* d = static_cast<nx_eth_descriptor_t*>(descriptor);
    if (hold_own && (d->status & own) != 0 && (value & own) == 0)
        return;
    d->status = value;
}
extern "C" void nx_eth_model_poll(void* context) {
    auto* mac = static_cast<nx_eth_mac_t*>(context);
    auto* regs = reinterpret_cast<uint32_t*>(mac->registers);
    ++now;
    elapsed_ns += 1000U;
    if (!hold_reset)
        regs[0x1000 / 4] &= ~1U;
    if (!hold_engine)
        regs[dma_status / 4] &= ~UINT32_C(0x7e0000);
    if (!hold_flush)
        regs[ctl / 4] &= ~UINT32_C(0x100000);
    if (!hold_mdio && (regs[0x10 / 4] & 1U)) {
        unsigned reg = (regs[0x10 / 4] >> 6U) & 31U;
        if (regs[0x10 / 4] & 2U) {
            phy[reg] = static_cast<uint16_t>(regs[0x14 / 4]);
            ++mdio_writes;
        } else
            regs[0x14 / 4] = phy[reg];
        regs[0x10 / 4] &= ~1U;
    }
}
#if defined(NX_ETH_TEST_STM32) || defined(NX_ETH_TEST_GD32)
extern "C" nx_result_t nx_eth_model_irq_control(nx_eth_mac_t*, bool) {
    return NX_SUCCESS;
}
extern "C" void nx_eth_model_pend_irq(nx_eth_mac_t*) {
}
extern "C" nx_result_t nx_eth_model_validate_wake(nx_eth_mac_t* mac,
                                                  const nx_irq_wake_t* wake,
                                                  uint8_t) {
    return nx_irq_wake_validate(wake, &mac->irq_policy, &mac->source);
}
#endif
/* D2-only. These two runtime regressions have NOT been compiled or executed.
 * The unchanged raw source is expected to fail them; root must record actual
 * runtime RED before modifying that existing implementation. */
TEST_F(EthernetMac, Es0182FourHclkReadsCannotProveFourTxRxClocksElapsed) {
    Start();
    /* RMII REF_CLK is50MHz. Even the fastest possible four-clock interval
     * is80ns; this deliberately does not assume the slower internal MII clock.
     * Do not time cold initialization with unmodeled CPU descriptor work. */
    last_mac_write_ns = 0U;
    enforce_spacing = true;
    const nx_eth_link_t down{false, true, 100U};
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_apply_link(&port, &down));
    EXPECT_EQ(0U, regs[0] & UINT32_C(0x0c));
    EXPECT_EQ(0U, internally_latched_mac & UINT32_C(0x0c));
}
TEST_F(EthernetMac, Es0182RequiresTxFifoEmptyBeforeFlushNotAfterIt) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_submit(&port, &request));
    regs[0x34U / 4U] = UINT32_C(0x01000000); /* TFNE, partial FIFO. */
    ASSERT_EQ(NX_SUCCESS, nx_eth_port_cancel(&port, &request));
    EXPECT_FALSE(illegal_flush);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
}

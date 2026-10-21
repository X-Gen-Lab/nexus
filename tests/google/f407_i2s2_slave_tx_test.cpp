/**
 * \file            f407_i2s2_slave_tx_test.cpp
 * \brief           Real finite I2S2 admission, prefill and retained frame loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "f407_i2s2_slave_tx_model.h"
#include "nexus/components/f407_i2s2_slave_tx.h"
extern "C" {
#include "stm32f407_system.h"
}
#include <atomic>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <vector>

extern "C" {
SPI_TypeDef g_f407_i2s_spi;
RCC_TypeDef g_f407_i2s_rcc;
GPIO_TypeDef g_f407_i2s_gpio;
DMA_TypeDef g_f407_i2s_dma;
DMA_Stream_TypeDef g_f407_i2s_stream;
TIM_TypeDef g_f407_i2s_timer;
nx_stm32_system_t g_nx_stm32_system;
}

namespace {
using testing::_;
using testing::NiceMock;
constexpr uintptr_t kSram = 0x20000000U;
constexpr size_t kSramBytes = 0x20000U;
constexpr uint32_t kWords = 8U;
constexpr uint32_t kErrorFlags = SPI_SR_UDR | SPI_SR_FRE;
constexpr uint32_t kDmaInterrupts =
    DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
enum class EventKind { Read, Write, Prefill, Withdraw };
struct Event {
    EventKind kind;
    volatile uint32_t* reg;
    uint32_t value;
    uint32_t tick;
};
nx_arch_irq_masks_t masks;
bool privileged;
uint32_t exception_number;

struct Hardware {
    MOCK_METHOD(uint32_t, Read, (volatile uint32_t*));
    MOCK_METHOD(void, Write, (volatile uint32_t*, uint32_t));
    std::vector<Event> events;
    DMA_Stream_TypeDef neighbor_stream = {};
    nx_f407_i2s2_tx_request_t* observed_request = nullptr;
    nx_frame_dma_loan_t* observed_loan = nullptr;
    bool frozen_timer = false;
    bool hold_dma_enable = false;
    bool hold_address = false;
    bool ignore_i2se_enable = false;
    bool ignore_txdmaen_set = false;
    bool refuse_irq_withdraw = false;
    bool irq_quiet = true;
    bool lose_ws_after_prefill = false;
    bool prefilled = false;
    bool wire_finishes_after_txe_read = false;
    bool accepted_before_dma = true;
    bool watch_status_after_admission = false;
    bool status_reads_after_borrow = true;
    bool prefetch_in_main_sram = true;
    bool loan_retained_during_access = true;
    bool detached_before_settlement = true;
    bool disabled_only_after_drain = true;
    unsigned withdrawals = 0U;
    unsigned prefill_count = 0U;
    unsigned software_stops = 0U;
    unsigned i2se_enable_attempts = 0U;
    uint32_t software_stop_remaining = 0U;
    unsigned txe_busy_samples = 0U;
    unsigned txe_idle_samples = 0U;
    unsigned status_reads = 0U;
    unsigned watched_status_reads = 0U;
    unsigned txe_sequence = 0U;
    unsigned txe_idle_sequence = 0U;
    uint32_t consumed_errors = 0U;
    uint32_t enabled_tick = 0U;
    uint32_t enable_readback_tick = 0U;
    bool enable_readback_seen = false;
    uint16_t prefetched = 0U;
    uint32_t ignore_shared_gate_set = 0U;
    uint32_t ignore_shared_gate_clear = 0U;
    unsigned system_polls = 0U;

    bool borrowed() const {
        return observed_request != nullptr && observed_loan != nullptr &&
               observed_loan->owner != nullptr &&
               observed_loan->identity == observed_loan &&
               nx_request_state(&observed_request->base) != NX_REQUEST_READY &&
               nx_request_state(&observed_request->base) != NX_REQUEST_SETTLED;
    }
    void observe_loan() {
        if (observed_request != nullptr &&
            nx_request_state(&observed_request->base) != NX_REQUEST_READY &&
            nx_request_state(&observed_request->base) != NX_REQUEST_SETTLED) {
            loan_retained_during_access &= borrowed();
        }
    }
    void prefill() {
        if (!prefilled && (g_f407_i2s_spi.I2SCFGR & SPI_I2SCFGR_I2SE) != 0U &&
            (g_f407_i2s_spi.I2SCFGR &
             (SPI_I2SCFGR_I2SCFG | SPI_I2SCFGR_I2SSTD | SPI_I2SCFGR_DATLEN |
              SPI_I2SCFGR_CHLEN)) == 0U &&
            (g_f407_i2s_spi.CR2 & SPI_CR2_TXDMAEN) != 0U &&
            (g_f407_i2s_stream.CR & DMA_SxCR_EN) != 0U &&
            (g_f407_i2s_stream.CR & DMA_SxCR_CHSEL) == 0U &&
            g_f407_i2s_stream.PAR ==
                static_cast<uint32_t>(
                    reinterpret_cast<uintptr_t>(&g_f407_i2s_spi.DR)) &&
            (g_f407_i2s_rcc.APB1ENR & RCC_APB1ENR_SPI2EN) != 0U &&
            (g_f407_i2s_rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN) != 0U &&
            (g_f407_i2s_spi.SR & SPI_SR_TXE) != 0U &&
            g_f407_i2s_stream.NDTR != 0U) {
            g_f407_i2s_spi.DR = prefetched;
            --g_f407_i2s_stream.NDTR;
            g_f407_i2s_spi.SR = SPI_SR_BSY;
            prefilled = true;
            ++prefill_count;
            events.push_back({EventKind::Prefill, &g_f407_i2s_spi.DR,
                              prefetched, g_f407_i2s_timer.CNT});
            if (lose_ws_after_prefill) {
                g_f407_i2s_gpio.IDR &= ~(1U << 12U);
            }
        }
    }
    uint32_t read_register(volatile uint32_t* reg) {
        observe_loan();
        const uint32_t value = *reg;
        events.push_back({EventKind::Read, reg, value, g_f407_i2s_timer.CNT});
        if (reg == &g_f407_i2s_spi.SR) {
            if (watch_status_after_admission) {
                ++watched_status_reads;
                status_reads_after_borrow &= borrowed();
                EXPECT_NE(nx_request_state(&observed_request->base),
                          NX_REQUEST_READY);
                EXPECT_TRUE(borrowed());
            }
            ++status_reads;
            consumed_errors |= value & kErrorFlags;
            g_f407_i2s_spi.SR &= ~kErrorFlags;
            if ((value & SPI_SR_TXE) != 0U) {
                if (txe_sequence == 0U) {
                    txe_sequence = status_reads;
                }
                if ((value & SPI_SR_BSY) != 0U) {
                    ++txe_busy_samples;
                    if (wire_finishes_after_txe_read) {
                        g_f407_i2s_spi.SR &= ~static_cast<uint32_t>(SPI_SR_BSY);
                        wire_finishes_after_txe_read = false;
                    }
                } else {
                    ++txe_idle_samples;
                    if (status_reads > txe_sequence) {
                        txe_idle_sequence = status_reads;
                    }
                }
            }
        }
        if (reg == &g_f407_i2s_spi.I2SCFGR &&
            (value & SPI_I2SCFGR_I2SE) != 0U && !enable_readback_seen) {
            enable_readback_seen = true;
            enable_readback_tick = g_f407_i2s_timer.CNT;
        }
        return value;
    }
    void write_register(volatile uint32_t* reg, uint32_t value) {
        observe_loan();
        events.push_back({EventKind::Write, reg, value, g_f407_i2s_timer.CNT});
        if (reg == &g_f407_i2s_rcc.AHB1ENR) {
            const uint32_t previous = *reg;
            const uint32_t ignored_set =
                value & ~previous & ignore_shared_gate_set;
            const uint32_t ignored_clear =
                previous & ~value & ignore_shared_gate_clear;
            *reg = (value & ~ignored_set) | ignored_clear;
        } else if (reg == &g_f407_i2s_rcc.AHB1RSTR &&
                   (value & RCC_AHB1RSTR_DMA1RST) != 0U) {
            neighbor_stream = {};
            std::memset(&g_f407_i2s_dma, 0, sizeof(g_f407_i2s_dma));
            std::memset(&g_f407_i2s_stream, 0, sizeof(g_f407_i2s_stream));
            g_f407_i2s_stream.FCR = 0x21U;
            *reg = value;
        } else if (reg == &g_f407_i2s_dma.HIFCR) {
            g_f407_i2s_dma.HISR &= ~value;
        } else if (reg == &g_f407_i2s_stream.CR) {
            if ((value & DMA_SxCR_EN) != 0U &&
                (g_f407_i2s_stream.CR & DMA_SxCR_EN) == 0U) {
                accepted_before_dma &=
                    borrowed() && observed_loan != nullptr &&
                    observed_loan->owner != nullptr &&
                    nx_request_state(&observed_request->base) ==
                        NX_REQUEST_QUEUED;
                /* Direct mode can read memory before a peripheral request. */
                const uintptr_t address = g_f407_i2s_stream.M0AR;
                const bool admitted = address >= kSram &&
                                      address <= kSram + kSramBytes - 2U &&
                                      address % 2U == 0U;
                prefetch_in_main_sram &= admitted;
                if (admitted) {
                    prefetched = *reinterpret_cast<const uint16_t*>(address);
                }
            }
            if (hold_dma_enable && (g_f407_i2s_stream.CR & DMA_SxCR_EN) != 0U) {
                value |= DMA_SxCR_EN;
            }
            if ((g_f407_i2s_stream.CR & DMA_SxCR_EN) != 0U &&
                (value & DMA_SxCR_EN) == 0U) {
                /* RM0090 10.3.13: software interruption also raises TCIF;
                 * the remaining peripheral count need not have reached zero. */
                g_f407_i2s_dma.HISR |= DMA_HISR_TCIF4;
                software_stop_remaining = g_f407_i2s_stream.NDTR;
                ++software_stops;
            }
            *reg = value;
        } else if (reg == &g_f407_i2s_stream.NDTR) {
            *reg = value;
            prefilled = false;
        } else if (reg == &g_f407_i2s_spi.I2SCFGR) {
            if ((value & SPI_I2SCFGR_I2SE) != 0U) {
                ++i2se_enable_attempts;
                if (ignore_i2se_enable) {
                    value &= ~static_cast<uint32_t>(SPI_I2SCFGR_I2SE);
                }
            }
            const bool was_enabled =
                (g_f407_i2s_spi.I2SCFGR & SPI_I2SCFGR_I2SE) != 0U;
            const bool enabled = (value & SPI_I2SCFGR_I2SE) != 0U;
            if (!was_enabled && enabled) {
                enabled_tick = g_f407_i2s_timer.CNT;
                enable_readback_seen = false;
                txe_busy_samples = txe_idle_samples = 0U;
                status_reads = txe_sequence = txe_idle_sequence = 0U;
                g_f407_i2s_spi.SR = SPI_SR_TXE;
            }
            if (was_enabled && !enabled) {
                disabled_only_after_drain &=
                    (g_f407_i2s_stream.CR & DMA_SxCR_EN) == 0U &&
                    txe_sequence != 0U && txe_idle_sequence > txe_sequence;
            }
            *reg = value;
            if (!enabled) {
                g_f407_i2s_spi.SR &=
                    ~static_cast<uint32_t>(SPI_SR_TXE | SPI_SR_BSY);
            }
        } else if (reg == &g_f407_i2s_spi.CR2) {
            *reg = ignore_txdmaen_set
                       ? value & ~static_cast<uint32_t>(SPI_CR2_TXDMAEN)
                       : value;
        } else if (reg == &g_f407_i2s_stream.FCR) {
            *reg =
                (value & ~static_cast<uint32_t>(DMA_SxFCR_FS)) |
                (g_f407_i2s_stream.FCR & static_cast<uint32_t>(DMA_SxFCR_FS));
        } else if ((reg == &g_f407_i2s_stream.M0AR ||
                    reg == &g_f407_i2s_stream.M1AR ||
                    reg == &g_f407_i2s_stream.PAR) &&
                   value == 0U) {
            if (*reg != 0U) {
                detached_before_settlement &= borrowed();
            }
            if (!hold_address) {
                *reg = value;
            }
        } else {
            *reg = value;
        }
        prefill();
    }
    bool withdraw() {
        observe_loan();
        ++withdrawals;
        events.push_back(
            {EventKind::Withdraw, nullptr, 0U, g_f407_i2s_timer.CNT});
        const bool sources_quiet =
            (g_f407_i2s_spi.CR2 &
             (SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN | SPI_CR2_TXEIE |
              SPI_CR2_RXNEIE | SPI_CR2_ERRIE)) == 0U &&
            (g_f407_i2s_stream.CR & (DMA_SxCR_EN | kDmaInterrupts)) == 0U &&
            (g_f407_i2s_stream.FCR & DMA_SxFCR_FEIE) == 0U;
        if (refuse_irq_withdraw || !sources_quiet) {
            return false;
        }
        irq_quiet = true;
        return true;
    }
    Hardware() {
        ON_CALL(*this, Read(_)).WillByDefault([this](volatile uint32_t* reg) {
            return read_register(reg);
        });
        ON_CALL(*this, Write(_, _))
            .WillByDefault([this](volatile uint32_t* reg, uint32_t value) {
                write_register(reg, value);
            });
    }
};
Hardware* hardware;

class F407I2s2 : public testing::Test {
  protected:
    NiceMock<Hardware> hw;
    nx_f407_i2s2_slave_tx_storage_t storage = {};
    const nx_f407_i2s2_slave_tx_t face = {&nx_f407_i2s2_slave_tx_ops, &storage};
    nx_f407_i2s2_slave_tx_config_t config = {NX_F407_I2S2_SLAVE_TX, 16U};
    nx_stream_slot_t slot = {};
    nx_stream_t stream = {};
    nx_frame_stream_t framed = {};
    nx_frame_dma_loan_t loan = {};
    nx_f407_i2s2_tx_request_t request = {};
    FLASH_TypeDef flash = {};
    PWR_TypeDef power = {};
    uint8_t* source = reinterpret_cast<uint8_t*>(kSram);
    void* mapping = MAP_FAILED;

    void clock_profile() {
        g_f407_i2s_rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSEON |
                            RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        g_f407_i2s_rcc.PLLCFGR =
            8U | (336U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
        g_f407_i2s_rcc.CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL |
                              RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
        g_f407_i2s_rcc.APB1ENR = RCC_APB1ENR_TIM2EN;
        g_f407_i2s_rcc.AHB1ENR = RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_GPIOBEN;
        g_f407_i2s_timer.PSC = 83U;
        g_f407_i2s_timer.ARR = UINT32_MAX;
        g_f407_i2s_timer.CR1 = TIM_CR1_CEN;
        g_f407_i2s_timer.DIER = TIM_DIER_UIE;
        g_f407_i2s_timer.CNT = 10U;
        g_nx_stm32_system.rcc = &g_f407_i2s_rcc;
        g_nx_stm32_system.timer = &g_f407_i2s_timer;
        g_nx_stm32_system.core_hz = 168000000U;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.timer_error = NX_SUCCESS;
    }
    void cold_system(uint32_t initial_ahb1) {
        const nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true,
                                            true};
        ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
        hw.observed_request = nullptr;
        hw.observed_loan = nullptr;
        std::memset(&g_f407_i2s_rcc, 0, sizeof(g_f407_i2s_rcc));
        std::memset(&g_f407_i2s_timer, 0, sizeof(g_f407_i2s_timer));
        flash = {};
        power = {};
        g_f407_i2s_rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY | (16U << 3U);
        g_f407_i2s_rcc.PLLCFGR = 0x24003010U;
        g_f407_i2s_rcc.AHB1ENR = initial_ahb1;
        power.CR = PWR_CR_VOS;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = &g_f407_i2s_rcc;
        g_nx_stm32_system.flash = &flash;
        g_nx_stm32_system.power = &power;
        g_nx_stm32_system.timer = &g_f407_i2s_timer;
        g_nx_stm32_system.core_hz = 16000000U;
        hw.events.clear();
    }
    void frame(size_t bytes = kWords * 2U, size_t stride = 0U, size_t rows = 1U,
               uint8_t* data = nullptr) {
        if (loan.owner != nullptr) {
            const nx_dma_drain_facts_t proof = {loan.block.length, 0U, true,
                                                true, true};
            ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
        }
        slot = {};
        stream = {};
        framed = {};
        loan = {};
        slot.data = data == nullptr ? source : data;
        slot.capacity = (rows - 1U) * (stride == 0U ? bytes : stride) + bytes;
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        const nx_frame_plane_t plane = {0U, bytes,
                                        stride == 0U ? bytes : stride, rows};
        nx_frame_layout_t layout = {};
        ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
                  NX_SUCCESS);
        nx_stream_fill_t fill = {};
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
        request = {};
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_request_prepare(&request.base, 1000U), NX_SUCCESS);
        request.source = &loan;
        hw.observed_request = &request;
        hw.observed_loan = &loan;
    }
    void SetUp() override {
        hardware = &hw;
        masks = {};
        privileged = true;
        exception_number = 0U;
        std::memset(&g_f407_i2s_spi, 0, sizeof(g_f407_i2s_spi));
        std::memset(&g_f407_i2s_rcc, 0, sizeof(g_f407_i2s_rcc));
        std::memset(&g_f407_i2s_gpio, 0, sizeof(g_f407_i2s_gpio));
        std::memset(&g_f407_i2s_dma, 0, sizeof(g_f407_i2s_dma));
        std::memset(&g_f407_i2s_stream, 0, sizeof(g_f407_i2s_stream));
        std::memset(&g_f407_i2s_timer, 0, sizeof(g_f407_i2s_timer));
        g_nx_stm32_system = {};
        clock_profile();
        g_f407_i2s_spi.SR = SPI_SR_TXE;
        g_f407_i2s_spi.I2SPR = 2U;
        g_f407_i2s_stream.FCR = 0x21U;
        g_f407_i2s_gpio.IDR = 1U << 12U;
        mapping =
            mmap(source, kSramBytes, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        ASSERT_EQ(mapping, source);
        std::memset(mapping, 0, kSramBytes);
        source[0] = 0x34U;
        source[1] = 0x12U;
        frame();
    }
    void TearDown() override {
        EXPECT_EQ(masks.primask, 0U);
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, kSramBytes), 0);
        }
        hardware = nullptr;
    }
    void initialize() {
        ASSERT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, &face, &config),
                  NX_SUCCESS);
    }
    void submit() {
        ASSERT_EQ(face.ops->submit(face.context, &request), NX_SUCCESS);
    }
    void arm() {
        submit();
        for (unsigned i = 0U;
             i < 4U && nx_request_state(&request.base) == NX_REQUEST_QUEUED;
             ++i) {
            (void)face.ops->service(face.context);
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    }
    void memory_complete() {
        g_f407_i2s_stream.NDTR = 0U;
        g_f407_i2s_stream.CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        g_f407_i2s_dma.HISR |= DMA_HISR_TCIF4;
        g_f407_i2s_spi.SR = SPI_SR_BSY;
    }
    void finish_wire() {
        memory_complete();
        g_f407_i2s_spi.SR = SPI_SR_TXE | SPI_SR_BSY;
        hw.wire_finishes_after_txe_read = true;
    }
    void settle(nx_result_t result = NX_SUCCESS) {
        if (result == NX_SUCCESS) {
            finish_wire();
        } else {
            /* Coordinated drain clocks empty DR after DMA has stopped. They
             * neither consume the remaining buffer nor synthesize DMA TC. */
            g_f407_i2s_spi.SR = SPI_SR_TXE | SPI_SR_BSY;
            hw.wire_finishes_after_txe_read = true;
        }
        for (unsigned i = 0U;
             i < 4U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i) {
            (void)face.ops->service(face.context);
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(request.base.result, result);
        if (result != NX_SUCCESS) {
            EXPECT_EQ(request.base.transferred, 0U);
        }
        EXPECT_EQ(loan.owner, nullptr);
        EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    }
    void retained() {
        EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_READY);
        EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        EXPECT_EQ(loan.owner, &framed);
        EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
        EXPECT_EQ(stream.outstanding, 1U);
        EXPECT_EQ(nx_request_prepare(&request.base, 2000U), NX_ERROR_STATE);
    }
    void reject_without_effects(nx_result_t expected) {
        EXPECT_CALL(hw, Write(_, _)).Times(0);
        EXPECT_EQ(face.ops->submit(face.context, &request), expected);
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
        EXPECT_EQ(loan.owner, &framed);
        EXPECT_EQ(g_f407_i2s_stream.M0AR, 0U);
        testing::Mock::VerifyAndClearExpectations(&hw);
    }
};

TEST_F(F407I2s2, ArmsOnlyAfterAcceptanceOneDrWordAndThreeLiveTim2Ticks) {
    initialize();
    arm();
    EXPECT_TRUE(hw.accepted_before_dma);
    EXPECT_TRUE(hw.prefetch_in_main_sram);
    EXPECT_EQ(hw.prefill_count, 1U);
    EXPECT_EQ(g_f407_i2s_spi.DR, 0x1234U);
    EXPECT_EQ(g_f407_i2s_stream.NDTR, kWords - 1U);
    EXPECT_EQ(g_f407_i2s_spi.SR & SPI_SR_TXE, 0U);
    EXPECT_NE(g_f407_i2s_spi.SR & SPI_SR_BSY, 0U);
    ASSERT_TRUE(hw.enable_readback_seen);
    EXPECT_GE(g_f407_i2s_timer.CNT - hw.enable_readback_tick, 3U);
    EXPECT_EQ(g_f407_i2s_stream.CR &
                  (DMA_SxCR_CIRC | DMA_SxCR_DBM | DMA_SxCR_PINC),
              0U);
    EXPECT_EQ(
        g_f407_i2s_stream.CR &
            (DMA_SxCR_DIR | DMA_SxCR_PSIZE | DMA_SxCR_MSIZE | DMA_SxCR_MINC),
        DMA_SxCR_DIR_0 | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_MINC);
    EXPECT_EQ(g_f407_i2s_stream.FCR & DMA_SxFCR_DMDIS, 0U);
    EXPECT_EQ(g_f407_i2s_stream.CR & DMA_SxCR_CHSEL, 0U);
    EXPECT_EQ(g_f407_i2s_stream.M0AR, static_cast<uint32_t>(kSram));
    EXPECT_EQ(
        g_f407_i2s_stream.PAR,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_f407_i2s_spi.DR)));
    EXPECT_EQ(g_f407_i2s_spi.I2SCFGR &
                  (SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SE | SPI_I2SCFGR_I2SCFG |
                   SPI_I2SCFGR_I2SSTD | SPI_I2SCFGR_DATLEN | SPI_I2SCFGR_CHLEN |
                   SPI_I2SCFGR_CKPOL),
              SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SE);
    EXPECT_EQ(g_f407_i2s_rcc.CR & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY), 0U);
    settle();
    EXPECT_EQ(request.base.transferred, kWords * 2U);
    EXPECT_TRUE(hw.loan_retained_during_access);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, DmaTcAndZeroCountNeverReleaseTheWholeFrameLoan) {
    initialize();
    arm();
    memory_complete();
    (void)face.ops->service(face.context);
    retained();
    EXPECT_NE(g_f407_i2s_spi.I2SCFGR & SPI_I2SCFGR_I2SE, 0U);
}

TEST_F(F407I2s2, DeadlineStillExpiresWhileCompletedDmaWaitsForWireDrain) {
    initialize();
    arm();
    memory_complete();
    g_f407_i2s_spi.SR = SPI_SR_TXE | SPI_SR_BSY;
    ASSERT_LT(nx_time_now_us(), request.base.deadline);
    (void)face.ops->service(face.context);
    retained();
    ASSERT_LT(nx_time_now_us(), request.base.deadline);
    EXPECT_NE(g_f407_i2s_spi.SR & SPI_SR_BSY, 0U);
    g_f407_i2s_timer.CNT = static_cast<uint32_t>(request.base.deadline + 1U);
    (void)face.ops->service(face.context);
    retained();
    EXPECT_NE(g_f407_i2s_spi.SR & SPI_SR_BSY, 0U);
    settle(NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(F407I2s2, FirstServiceAfterDeadlineCannotTurnQuietDmaTcIntoSuccess) {
    initialize();
    arm();
    memory_complete();
    g_f407_i2s_spi.SR = SPI_SR_TXE;
    g_f407_i2s_timer.CNT = static_cast<uint32_t>(request.base.deadline + 1U);
    for (unsigned i = 0U;
         i < 4U && nx_request_state(&request.base) != NX_REQUEST_SETTLED; ++i) {
        (void)face.ops->service(face.context);
    }
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
}

TEST_F(F407I2s2, FreshTxeThenBsyAndReferenceWithdrawalPrecedeFinalAccess) {
    initialize();
    arm();
    hw.withdrawals = 0U;
    hw.hold_address = true;
    finish_wire();
    (void)face.ops->service(face.context);
    retained();
    EXPECT_NE(g_f407_i2s_stream.M0AR, 0U);
    hw.hold_address = false;
    for (unsigned i = 0U;
         i < 4U && nx_request_state(&request.base) != NX_REQUEST_SETTLED; ++i) {
        (void)face.ops->service(face.context);
    }
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_TRUE(hw.disabled_only_after_drain);
    EXPECT_TRUE(hw.detached_before_settlement);
    EXPECT_TRUE(hw.loan_retained_during_access);
    EXPECT_GT(hw.withdrawals, 0U);
    EXPECT_EQ(g_f407_i2s_stream.M0AR, 0U);
    EXPECT_EQ(g_f407_i2s_stream.M1AR, 0U);
    EXPECT_EQ(g_f407_i2s_stream.PAR, 0U);
    EXPECT_EQ(g_f407_i2s_spi.I2SCFGR & SPI_I2SCFGR_I2SE, 0U);
    EXPECT_EQ(face.ops->service(face.context), NX_SUCCESS);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2,
       PrefilledCancellationQuarantinesUntilMasterSuppliesDrainClocks) {
    initialize();
    arm();
    EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    (void)face.ops->service(face.context);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    retained();
    EXPECT_GT(hw.software_stops, 0U);
    EXPECT_EQ(hw.software_stop_remaining, kWords - 1U);
    EXPECT_EQ(g_f407_i2s_stream.NDTR, kWords - 1U);
    g_f407_i2s_dma.HISR |= DMA_HISR_TCIF4;
    (void)face.ops->service(face.context);
    retained();
    EXPECT_NE(face.ops->stop(face.context), NX_SUCCESS);
    settle(NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, FrozenTim2CannotTurnPollBudgetIntoAnArmedTransfer) {
    initialize();
    hw.frozen_timer = true;
    submit();
    (void)face.ops->service(face.context);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    retained();
    EXPECT_EQ(hw.prefill_count, 1U);
    EXPECT_EQ(g_f407_i2s_timer.CNT, hw.enabled_tick);
    EXPECT_NE(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, WsLostAfterAdmissionRetainsAcceptedSource) {
    initialize();
    hw.lose_ws_after_prefill = true;
    submit();
    (void)face.ops->service(face.context);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    retained();
    EXPECT_TRUE(hw.accepted_before_dma);
    EXPECT_EQ(hw.prefill_count, 1U);
}

TEST_F(F407I2s2, RefusedDmaDisableReadbackRetainsAcceptedSource) {
    initialize();
    arm();
    hw.hold_dma_enable = true;
    EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    (void)face.ops->service(face.context);
    retained();
    EXPECT_NE(g_f407_i2s_stream.CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(face.ops->stop(face.context), NX_SUCCESS);
    hw.hold_dma_enable = false;
    settle(NX_ERROR_CANCELLED);
}

TEST_F(F407I2s2, RejectsIncompleteStereoGeometryBeforeAdmissionOrMmioWrites) {
    initialize();
    frame(6U);
    reject_without_effects(NX_ERROR_INVALID);
}

TEST_F(F407I2s2, CanonicalTwoFrameAudioLayoutKeepsItsRowsAndWholeLoan) {
    initialize();
    const nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true,
                                        true};
    ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    const nx_frame_audio_format_t format = {NX_FRAME_AUDIO_I2S,
                                            NX_FRAME_PCM_SIGNED,
                                            NX_FRAME_SAMPLE_LSB,
                                            2U,
                                            3U,
                                            2U,
                                            16U,
                                            16U,
                                            false};
    nx_frame_layout_t layout = {};
    ASSERT_EQ(nx_frame_layout_audio(&format, &layout), NX_SUCCESS);
    ASSERT_EQ(layout.plane_count, 1U);
    ASSERT_EQ(layout.planes[0].row_bytes, 4U);
    ASSERT_EQ(layout.planes[0].stride, 4U);
    ASSERT_EQ(layout.planes[0].rows, 2U);
    ASSERT_EQ(layout.bytes, 8U);
    slot = {};
    stream = {};
    framed = {};
    loan = {};
    slot.data = source;
    slot.capacity = layout.bytes;
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
              NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    arm();
    EXPECT_EQ(g_f407_i2s_stream.NDTR, 3U);
    memory_complete();
    (void)face.ops->service(face.context);
    retained();
    settle();
    EXPECT_EQ(request.base.transferred, 8U);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, RejectsEmptyOverflowedAndInexactContiguousRowExtents) {
    initialize();
    const nx_frame_layout_t original = framed.layout;
    for (unsigned geometry = 0U; geometry < 4U; ++geometry) {
        SCOPED_TRACE(geometry);
        framed.layout = original;
        nx_frame_plane_t& plane = framed.layout.planes[0];
        if (geometry == 0U) {
            plane.rows = 0U;
        } else if (geometry == 1U) {
            plane.row_bytes = 0U;
            plane.stride = 0U;
        } else if (geometry == 2U) {
            /* Unchecked multiplication wraps back to the old 16-byte span. */
            plane.row_bytes = SIZE_MAX / 2U + 9U;
            plane.stride = plane.row_bytes;
            plane.rows = 2U;
        } else {
            plane.row_bytes = 4U;
            plane.stride = 4U;
            plane.rows = 2U;
        }
        reject_without_effects(NX_ERROR_INVALID);
    }
}

TEST_F(F407I2s2, RejectsPaddedMultipleRowsBeforeAdmissionOrMmioWrites) {
    initialize();
    frame(8U, 12U, 2U);
    reject_without_effects(NX_ERROR_INVALID);
}

TEST_F(F407I2s2, RejectsCpuOnlyCcmBeforeDmaPrefetchCanReadIt) {
    initialize();
    frame(kWords * 2U, 0U, 1U, reinterpret_cast<uint8_t*>(0x10000000U));
    reject_without_effects(NX_ERROR_PERMISSION);
    EXPECT_EQ(hw.prefill_count, 0U);
}

TEST_F(F407I2s2,
       RejectsHandlerPrivilegeAndEveryIncomingMaskWithoutChangingThem) {
    initialize();
    for (unsigned context = 0U; context < 5U; ++context) {
        SCOPED_TRACE(context);
        privileged = context != 0U;
        exception_number = context == 1U ? 16U : 0U;
        masks = {context == 2U ? 1U : 0U, context == 3U ? 32U : 0U,
                 context == 4U ? 1U : 0U};
        const auto incoming = masks;
        reject_without_effects(NX_ERROR_CONTEXT);
        EXPECT_EQ(masks.primask, incoming.primask);
        EXPECT_EQ(masks.basepri, incoming.basepri);
        EXPECT_EQ(masks.faultmask, incoming.faultmask);
    }
    masks = {};
    privileged = true;
    exception_number = 0U;
}

TEST_F(F407I2s2, RejectsForeignSpiPersonalityAndUnownedSpi2GateWithoutWrites) {
    EXPECT_CALL(hw, Write(_, _)).Times(0);
    g_f407_i2s_rcc.APB1ENR |= RCC_APB1ENR_SPI2EN;
    g_f407_i2s_spi.CR1 = SPI_CR1_SPE;
    EXPECT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, &face, &config),
              NX_ERROR_BUSY);
    g_f407_i2s_spi.CR1 = 0U;
    g_f407_i2s_rcc.APB1ENR |= RCC_APB1ENR_SPI2EN;
    EXPECT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, &face, &config),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(F407I2s2, FirstStatusReadLatchesEveryReadToClearErrorBeforeDrain) {
    initialize();
    for (const uint32_t error : {static_cast<uint32_t>(SPI_SR_UDR),
                                 static_cast<uint32_t>(SPI_SR_FRE)}) {
        SCOPED_TRACE(error);
        arm();
        g_f407_i2s_spi.SR |= error;
        (void)face.ops->service(face.context);
        retained();
        EXPECT_EQ(hw.consumed_errors & error, error);
        EXPECT_EQ(g_f407_i2s_spi.SR & error, 0U);
        settle(NX_ERROR_IO);
        frame();
    }
}

TEST_F(F407I2s2, RefusedVectorWithdrawalRetainsLoanAfterSerializerDrains) {
    initialize();
    arm();
    hw.withdrawals = 0U;
    hw.refuse_irq_withdraw = true;
    finish_wire();
    (void)face.ops->service(face.context);
    retained();
    EXPECT_GT(hw.withdrawals, 0U);
    hw.refuse_irq_withdraw = false;
    for (unsigned i = 0U;
         i < 4U && nx_request_state(&request.base) != NX_REQUEST_SETTLED; ++i) {
        (void)face.ops->service(face.context);
    }
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_TRUE(hw.loan_retained_during_access);
}

TEST_F(F407I2s2,
       ObservedSerializerResetPermanentlyRetainsErasedTransferHistory) {
    initialize();
    arm();
    g_f407_i2s_rcc.APB1RSTR |= RCC_APB1RSTR_SPI2RST;
    (void)face.ops->service(face.context);
    retained();
    g_f407_i2s_rcc.APB1RSTR &= ~static_cast<uint32_t>(RCC_APB1RSTR_SPI2RST);
    finish_wire();
    (void)face.ops->service(face.context);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    retained();
    EXPECT_NE(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, ObservedDmaGateLossPermanentlyRetainsErasedTransferHistory) {
    initialize();
    arm();
    g_f407_i2s_rcc.AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_DMA1EN);
    (void)face.ops->service(face.context);
    retained();
    g_f407_i2s_rcc.AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    finish_wire();
    (void)face.ops->service(face.context);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    retained();
    EXPECT_NE(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, RejectsCounterOverflowAndOddStereoThenAcceptsMaximumBlock) {
    initialize();
    for (const size_t bytes : {131070U, 131072U}) {
        SCOPED_TRACE(bytes);
        frame(bytes);
        reject_without_effects(NX_ERROR_INVALID);
    }
    frame(131068U);
    arm();
    EXPECT_EQ(g_f407_i2s_stream.NDTR, 65533U);
    settle();
    EXPECT_EQ(request.base.transferred, 131068U);
}

TEST_F(F407I2s2, CopiedAndStaleExactLoanControlsRejectWithoutAdmission) {
    initialize();
    const nx_frame_dma_loan_t previous = loan;
    nx_frame_dma_loan_t copied = loan;
    request.source = &copied;
    reject_without_effects(NX_ERROR_STATE);
    const nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true,
                                        true};
    ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    /* Reconstruct stale bytes at the original control address. Its identity
     * matches, but the real queue has already released that incarnation. */
    loan = previous;
    request.source = &loan;
    reject_without_effects(NX_ERROR_STATE);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(stream.outstanding, 0U);
}

TEST_F(F407I2s2, RequestInUnusedFullCapacityPoolTailRejectsBeforeAdmission) {
    initialize();
    const nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true,
                                        true};
    ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    slot = {};
    slot.data = source;
    slot.capacity = 128U;
    ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
    const nx_frame_plane_t plane = {0U, 16U, 16U, 1U};
    nx_frame_layout_t layout = {};
    ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
              NX_SUCCESS);
    auto* alias = reinterpret_cast<nx_f407_i2s2_tx_request_t*>(source + 64U);
    ASSERT_LE(sizeof(*alias), 64U);
    ::new (alias) nx_f407_i2s2_tx_request_t{};
    nx_request_initialize(&alias->base);
    ASSERT_EQ(nx_request_prepare(&alias->base, 1000U), NX_SUCCESS);
    alias->source = &loan;
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    hw.observed_request = alias;
    hw.observed_loan = &loan;
    EXPECT_CALL(hw, Write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, alias), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&alias->base), NX_REQUEST_READY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(loan.owner, &framed);
    EXPECT_EQ(g_f407_i2s_stream.M0AR, 0U);
}

TEST_F(F407I2s2, SettledRequestAndSourceControlMayBecomeInaccessible) {
    initialize();
    struct Controls {
        nx_f407_i2s2_tx_request_t request;
        nx_frame_dma_loan_t loan;
    };
    void* page = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);
    auto* isolated = ::new (page) Controls{};
    const nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true,
                                        true};
    ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &isolated->loan), NX_SUCCESS);
    nx_request_initialize(&isolated->request.base);
    ASSERT_EQ(nx_request_prepare(&isolated->request.base, 1000U), NX_SUCCESS);
    isolated->request.source = &isolated->loan;
    hw.observed_request = &isolated->request;
    hw.observed_loan = &isolated->loan;
    ASSERT_EQ(face.ops->submit(face.context, &isolated->request), NX_SUCCESS);
    for (unsigned i = 0U; i < 4U && nx_request_state(&isolated->request.base) ==
                                        NX_REQUEST_QUEUED;
         ++i) {
        (void)face.ops->service(face.context);
    }
    ASSERT_EQ(nx_request_state(&isolated->request.base), NX_REQUEST_ACTIVE);
    finish_wire();
    for (unsigned i = 0U; i < 4U && nx_request_state(&isolated->request.base) !=
                                        NX_REQUEST_SETTLED;
         ++i) {
        (void)face.ops->service(face.context);
    }
    ASSERT_EQ(nx_request_state(&isolated->request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(isolated->request.base.result, NX_SUCCESS);
    EXPECT_EQ(isolated->request.base.transferred, kWords * 2U);
    EXPECT_EQ(isolated->loan.owner, nullptr);
    hw.observed_request = nullptr;
    hw.observed_loan = nullptr;
    ASSERT_EQ(mprotect(page, 4096U, PROT_NONE), 0);
    EXPECT_EQ(face.ops->service(face.context), NX_SUCCESS);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    EXPECT_EQ(munmap(page, 4096U), 0);
}

TEST_F(F407I2s2, UnconfirmedI2seAttemptNeverBecomesProofOfNeverEnabled) {
    initialize();
    hw.ignore_i2se_enable = true;
    submit();
    (void)face.ops->service(face.context);
    EXPECT_GT(hw.i2se_enable_attempts, 0U);
    EXPECT_EQ(g_f407_i2s_spi.I2SCFGR & SPI_I2SCFGR_I2SE, 0U);
    EXPECT_EQ(hw.prefill_count, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    retained();
    EXPECT_NE(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2,
       FailedDmaRequestBeforeI2seAttemptSettlesOnlyAfterActualCleanup) {
    initialize();
    /* A never-attempted serializer needs no TXE observation to prove that
     * no word started. Make the absent flag explicit for this failure path. */
    g_f407_i2s_spi.SR = 0U;
    hw.ignore_txdmaen_set = true;
    submit();
    for (unsigned i = 0U;
         i < 4U && nx_request_state(&request.base) != NX_REQUEST_SETTLED; ++i) {
        (void)face.ops->service(face.context);
    }
    EXPECT_EQ(hw.i2se_enable_attempts, 0U);
    EXPECT_EQ(hw.prefill_count, 0U);
    EXPECT_EQ(g_f407_i2s_spi.SR & SPI_SR_TXE, 0U);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
    EXPECT_EQ(loan.owner, nullptr);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    EXPECT_EQ(g_f407_i2s_stream.CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(g_f407_i2s_stream.M0AR, 0U);
    EXPECT_EQ(g_f407_i2s_stream.M1AR, 0U);
    EXPECT_EQ(g_f407_i2s_stream.PAR, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(F407I2s2, IdleReadToClearFaultIsObservedOnlyAfterAdmissionAndStaysIo) {
    initialize();
    g_f407_i2s_spi.SR |= SPI_SR_UDR | SPI_SR_FRE;
    hw.watch_status_after_admission = true;
    submit();
    for (unsigned i = 0U;
         i < 4U && nx_request_state(&request.base) != NX_REQUEST_SETTLED; ++i) {
        EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
        (void)face.ops->service(face.context);
    }
    hw.watch_status_after_admission = false;
    EXPECT_GT(hw.watched_status_reads, 0U);
    EXPECT_TRUE(hw.status_reads_after_borrow);
    EXPECT_EQ(hw.consumed_errors & kErrorFlags, kErrorFlags);
    EXPECT_EQ(g_f407_i2s_spi.SR & kErrorFlags, 0U);
    EXPECT_EQ(hw.i2se_enable_attempts, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    if (nx_request_state(&request.base) != NX_REQUEST_SETTLED) {
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
        retained();
    }
    /* A later status observation sees cleared hardware flags; it must not
     * replace the admitted transfer's first IO cause with success. */
    (void)face.ops->service(face.context);
    if (nx_request_state(&request.base) == NX_REQUEST_SETTLED) {
        EXPECT_EQ(request.base.result, NX_ERROR_IO);
        EXPECT_EQ(request.base.transferred, 0U);
        EXPECT_EQ(loan.owner, nullptr);
        EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
    } else {
        EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
        retained();
    }
}

TEST_F(F407I2s2, MissingPlatformDmaGateRejectsBeforeTouchingStaleDmaState) {
    g_f407_i2s_rcc.AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_DMA1EN);
    g_f407_i2s_stream.CR = DMA_SxCR_EN;
    const auto dma_register = [](volatile uint32_t* reg) {
        const uintptr_t address = reinterpret_cast<uintptr_t>(reg);
        const uintptr_t controller =
            reinterpret_cast<uintptr_t>(&g_f407_i2s_dma);
        const uintptr_t dma_stream_address =
            reinterpret_cast<uintptr_t>(&g_f407_i2s_stream);
        return (address >= controller &&
                address < controller + sizeof(g_f407_i2s_dma)) ||
               (address >= dma_stream_address &&
                address < dma_stream_address + sizeof(g_f407_i2s_stream));
    };
    EXPECT_CALL(hw, Read(_)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, Write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, Read(testing::Truly(dma_register))).Times(0);
    EXPECT_CALL(hw, Write(testing::Truly(dma_register), _)).Times(0);
    EXPECT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, &face, &config),
              NX_ERROR_STATE);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN, 0U);
    EXPECT_EQ(g_f407_i2s_stream.CR, DMA_SxCR_EN);
    for (const auto& event : hw.events) {
        if (event.kind == EventKind::Write &&
            event.reg == &g_f407_i2s_rcc.AHB1ENR) {
            EXPECT_EQ(event.value & RCC_AHB1ENR_DMA1EN, 0U);
        }
    }
}

TEST_F(F407I2s2, MissingPlatformGpioGateRejectsBeforeTouchingStalePadState) {
    g_f407_i2s_rcc.AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_GPIOBEN);
    g_f407_i2s_gpio.MODER = 0xaaaaaaaaU;
    const auto gpio_register = [](volatile uint32_t* reg) {
        const uintptr_t address = reinterpret_cast<uintptr_t>(reg);
        const uintptr_t gpio = reinterpret_cast<uintptr_t>(&g_f407_i2s_gpio);
        return address >= gpio && address < gpio + sizeof(g_f407_i2s_gpio);
    };
    EXPECT_CALL(hw, Read(_)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, Write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, Read(testing::Truly(gpio_register))).Times(0);
    EXPECT_CALL(hw, Write(testing::Truly(gpio_register), _)).Times(0);
    EXPECT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, &face, &config),
              NX_ERROR_STATE);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR & RCC_AHB1ENR_GPIOBEN, 0U);
    EXPECT_EQ(g_f407_i2s_gpio.MODER, 0xaaaaaaaaU);
    for (const auto& event : hw.events) {
        if (event.kind == EventKind::Write &&
            event.reg == &g_f407_i2s_rcc.AHB1ENR) {
            EXPECT_EQ(event.value & RCC_AHB1ENR_GPIOBEN, 0U);
        }
    }
}

TEST_F(F407I2s2, StopPreservesPlatformSharedGatesAndNeighborStreamAndPads) {
    hw.neighbor_stream.CR = DMA_SxCR_EN | DMA_SxCR_MINC;
    hw.neighbor_stream.NDTR = 19U;
    hw.neighbor_stream.PAR = 0x40004404U;
    hw.neighbor_stream.M0AR = 0x20010000U;
    g_f407_i2s_dma.LISR = DMA_LISR_TCIF0;
    g_f407_i2s_dma.HISR = DMA_HISR_TCIF5;
    g_f407_i2s_gpio.MODER = (1U << 12U) | (2U << 28U);
    g_f407_i2s_gpio.OTYPER = (1U << 6U) | (1U << 14U);
    g_f407_i2s_gpio.OSPEEDR = (3U << 12U) | (1U << 28U);
    g_f407_i2s_gpio.PUPDR = (1U << 12U) | (2U << 28U);
    g_f407_i2s_gpio.ODR = (1U << 6U) | (1U << 14U);
    g_f407_i2s_gpio.AFR[0] = 5U << 24U;
    g_f407_i2s_gpio.AFR[1] = 9U << 24U;
    GPIO_TypeDef pins_before;
    DMA_Stream_TypeDef stream_before;
    std::memcpy(&pins_before, &g_f407_i2s_gpio, sizeof(pins_before));
    std::memcpy(&stream_before, &hw.neighbor_stream, sizeof(stream_before));
    initialize();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    const uint32_t gates = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR & gates, gates);
    EXPECT_EQ(std::memcmp(&pins_before, &g_f407_i2s_gpio, sizeof(pins_before)),
              0);
    EXPECT_EQ(
        std::memcmp(&stream_before, &hw.neighbor_stream, sizeof(stream_before)),
        0);
    EXPECT_EQ(g_f407_i2s_dma.LISR, DMA_LISR_TCIF0);
    EXPECT_EQ(g_f407_i2s_dma.HISR, DMA_HISR_TCIF5);
    for (const auto& event : hw.events) {
        if (event.kind != EventKind::Write) {
            continue;
        }
        if (event.reg == &g_f407_i2s_rcc.AHB1ENR) {
            EXPECT_EQ(event.value & gates, gates);
        }
        if (event.reg == &g_f407_i2s_rcc.AHB1RSTR) {
            EXPECT_EQ(event.value &
                          (RCC_AHB1RSTR_DMA1RST | RCC_AHB1RSTR_GPIOBRST),
                      0U);
        }
    }
}

TEST_F(F407I2s2, UnalignedFaceAndConfigRejectBeforeControlOrHardwareUse) {
    alignas(nx_f407_i2s2_slave_tx_t) unsigned char
        face_bytes[sizeof(face) + alignof(nx_f407_i2s2_slave_tx_t)] = {};
    alignas(nx_f407_i2s2_slave_tx_config_t) unsigned char
        config_bytes[sizeof(config) + alignof(nx_f407_i2s2_slave_tx_config_t)] =
            {};
    std::memcpy(face_bytes + 1U, &face, sizeof(face));
    std::memcpy(config_bytes + 1U, &config, sizeof(config));
    const auto* bad_face =
        reinterpret_cast<const nx_f407_i2s2_slave_tx_t*>(face_bytes + 1U);
    const auto* bad_config =
        reinterpret_cast<const nx_f407_i2s2_slave_tx_config_t*>(config_bytes +
                                                                1U);
    EXPECT_CALL(hw, Read(_)).Times(0);
    EXPECT_CALL(hw, Write(_, _)).Times(0);
    EXPECT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, bad_face, &config),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_f407_i2s2_slave_tx_initialize(&storage, &face, bad_config),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}

TEST_F(F407I2s2, UnalignedLoanControlRejectsBeforeDereferenceOrAdmission) {
    initialize();
    alignas(nx_frame_dma_loan_t) unsigned char
        bytes[sizeof(loan) + alignof(nx_frame_dma_loan_t)] = {};
    std::memcpy(bytes + 1U, &loan, sizeof(loan));
    request.source = reinterpret_cast<nx_frame_dma_loan_t*>(bytes + 1U);
    reject_without_effects(NX_ERROR_INVALID);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(loan.owner, &framed);
}

TEST_F(F407I2s2, RealSelectedSystemOwnsAddedSharedGatesUntilConsumersJoin) {
    cold_system(RCC_AHB1ENR_GPIOAEN);
    ASSERT_EQ(nx_stm32_system_preflight(&g_nx_stm32_system), NX_SUCCESS);
    const auto started = nx_stm32_system_start(&g_nx_stm32_system, 16U);
    ASSERT_EQ(started.primary, 0);
    EXPECT_EQ(started.cleanup, 0);
    EXPECT_TRUE(g_nx_stm32_system.started);
    const uint32_t shared = RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR & shared, shared);
    EXPECT_GE(hw.system_polls, 4U);
    EXPECT_EQ(g_f407_i2s_timer.PSC, 83U);
    EXPECT_EQ(g_f407_i2s_timer.ARR, UINT32_MAX);
    EXPECT_EQ(g_f407_i2s_timer.EGR, 0U);
    EXPECT_EQ(g_f407_i2s_timer.SR & TIM_SR_UIF, 0U);
    if ((g_f407_i2s_rcc.AHB1ENR & shared) == shared) {
        initialize();
        EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
        EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR & shared, shared);
    }
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 16U), 0);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR, RCC_AHB1ENR_GPIOAEN);
    EXPECT_EQ(g_f407_i2s_timer.CR1 & TIM_CR1_CEN, 0U);
    for (const auto& event : hw.events) {
        if (event.kind == EventKind::Write &&
            event.reg == &g_f407_i2s_rcc.AHB1RSTR) {
            EXPECT_EQ(event.value & RCC_AHB1RSTR_DMA1RST, 0U);
        }
    }
}

TEST_F(F407I2s2, RealSelectedSystemPreservesPreexistingSharedGateOwners) {
    const uint32_t initial =
        RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    cold_system(initial);
    const auto started = nx_stm32_system_start(&g_nx_stm32_system, 16U);
    ASSERT_EQ(started.primary, 0);
    EXPECT_EQ(started.cleanup, 0);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR, initial);
    initialize();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 16U), 0);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR, initial);
    for (const auto& event : hw.events) {
        if (event.kind == EventKind::Write &&
            event.reg == &g_f407_i2s_rcc.AHB1ENR) {
            EXPECT_EQ(event.value & initial, initial);
        }
    }
}

TEST_F(F407I2s2, RealSystemRetainsFailedPartialSharedGateRollbackUntilRetry) {
    cold_system(RCC_AHB1ENR_GPIOAEN);
    hw.ignore_shared_gate_set = RCC_AHB1ENR_DMA1EN;
    hw.ignore_shared_gate_clear = RCC_AHB1ENR_GPIOBEN;
    const auto started = nx_stm32_system_start(&g_nx_stm32_system, 16U);
    EXPECT_NE(started.primary, 0);
    EXPECT_NE(started.cleanup, 0);
    EXPECT_NE(started.remaining_effects, 0U);
    EXPECT_EQ(started.remaining_effects, g_nx_stm32_system.remaining_effects);
    EXPECT_FALSE(g_nx_stm32_system.started);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN, 0U);
    EXPECT_NE(g_f407_i2s_rcc.AHB1ENR & RCC_AHB1ENR_GPIOBEN, 0U);
    EXPECT_NE(g_f407_i2s_rcc.AHB1ENR & RCC_AHB1ENR_GPIOAEN, 0U);
    hw.ignore_shared_gate_set = 0U;
    hw.ignore_shared_gate_clear = 0U;
    EXPECT_EQ(nx_stm32_system_stop(&g_nx_stm32_system, 16U), 0);
    EXPECT_EQ(g_nx_stm32_system.remaining_effects, 0U);
    EXPECT_EQ(g_f407_i2s_rcc.AHB1ENR, RCC_AHB1ENR_GPIOAEN);
}
} /* namespace */

extern "C" uint32_t nx_f407_i2s_model_read(volatile uint32_t* reg) {
    return hardware->Read(reg);
}
extern "C" void nx_f407_i2s_model_write(volatile uint32_t* reg,
                                        uint32_t value) {
    hardware->Write(reg, value);
}
extern "C" void nx_f407_i2s_model_poll(void) {
    if (!hardware->frozen_timer && (g_f407_i2s_timer.CR1 & TIM_CR1_CEN) != 0U) {
        ++g_f407_i2s_timer.CNT;
    }
    hardware->prefill();
}
extern "C" void nx_f407_i2s_model_system_poll(void* raw) {
    auto* system = static_cast<nx_stm32_system_t*>(raw);
    EXPECT_EQ(system->rcc, &g_f407_i2s_rcc);
    EXPECT_EQ(system->timer, &g_f407_i2s_timer);
    ++hardware->system_polls;
    auto* rcc = system->rcc;
    uint32_t control =
        rcc->CR &
        ~static_cast<uint32_t>(RCC_CR_HSIRDY | RCC_CR_HSERDY | RCC_CR_PLLRDY);
    if ((control & RCC_CR_HSION) != 0U) {
        control |= RCC_CR_HSIRDY;
    }
    if ((control & RCC_CR_HSEON) != 0U) {
        control |= RCC_CR_HSERDY;
    }
    if ((control & RCC_CR_PLLON) != 0U) {
        control |= RCC_CR_PLLRDY;
    }
    rcc->CR = control;
    const uint32_t requested = rcc->CFGR & RCC_CFGR_SW;
    uint32_t status = rcc->CFGR & RCC_CFGR_SWS;
    if (requested == RCC_CFGR_SW_HSI && (control & RCC_CR_HSIRDY) != 0U) {
        status = 0U;
    } else if (requested == RCC_CFGR_SW_PLL &&
               (control & RCC_CR_PLLRDY) != 0U) {
        status = RCC_CFGR_SWS_PLL;
    }
    rcc->CFGR = (rcc->CFGR & ~static_cast<uint32_t>(RCC_CFGR_SWS)) | status;
    auto* timer = system->timer;
    if ((timer->EGR & TIM_EGR_UG) != 0U) {
        timer->SR |= TIM_SR_UIF;
        timer->EGR &= ~static_cast<uint32_t>(TIM_EGR_UG);
    }
    if (!hardware->frozen_timer && (timer->CR1 & TIM_CR1_CEN) != 0U) {
        ++timer->CNT;
    }
}
extern "C" bool nx_f407_i2s_model_irq_quiet(void) {
    return hardware->irq_quiet;
}
extern "C" bool nx_f407_i2s_model_irq_withdraw(void) {
    return hardware->withdraw();
}
extern "C" uint16_t nx_f407_i2s_model_flash_kib(void) {
#ifdef NX_F407_I2S_FLASH_BYTES
    return NX_F407_I2S_FLASH_BYTES / 1024U;
#else
    return 512U;
#endif
}
extern "C" uint32_t nx_f407_i2s_model_device_id(void) {
    return 0x10000413U;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous = {masks.primask};
    masks.primask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    masks.primask = previous.value;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return masks;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masks.primask != 0U || masks.basepri != 0U || masks.faultmask != 0U;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return exception_number;
}
extern "C" bool nx_arch_in_isr(void) {
    return exception_number != 0U;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
/* Synchronous MMIO model: compiler ordering, no thread publication. */
extern "C" void nx_arch_dmb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" void nx_arch_dsb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" void nx_arch_isb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

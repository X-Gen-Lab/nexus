/**
 * \file            stm32_ltdc_test.cpp
 * \brief           Actual HAL scanout handover and retained frame contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_ltdc_model.h"
extern "C" {
#include "stm32f407_system.h"
#include "stm32f4_graphics_memory.h"
#include "stm32f4_ltdc.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <sys/mman.h>

namespace {
struct Hardware {
    RCC_TypeDef rcc{};
    GPIO_TypeDef gpio[9]{};
    LTDC_TypeDef registers{};
    LTDC_Layer_TypeDef active[2]{}, shadow[2]{};
    uint32_t enabled[93]{}, pending[93]{}, in_irq[93]{}, priority[93]{};
    nx_time_us_t now = 1;
    bool privileged = true, in_isr = false, hold_reload = false;
    bool hold_disable = false, hold_irq = false, hold_priority = false;
    bool refuse_enable = false;
    bool refuse_gate_enable = false, refuse_gate_disable = false;
    bool refuse_pad_setup = false, refuse_pad_release = false;
    unsigned writes = 0;
    unsigned graphics_attaches = 0, graphics_checks = 0, graphics_detaches = 0;
    alignas(8) uint8_t bank_metadata[64]{};
    nx_result_t attach_result = NX_SUCCESS, check_result = NX_SUCCESS;
    nx_result_t detach_result = NX_SUCCESS;
} * hardware;

class LtdcOwner : public testing::Test {
  protected:
    Hardware model{};
    uint8_t* memory = nullptr;
    nx_stm32_system_t* system = nullptr;
    nx_dma_memory_region_t region{};
    /* Retained graphics metadata belongs to the whole fixture lifetime. */
    nx_stm32_sdram_graphics_t graphics_binding{};
    nx_stm32_ltdc_config_t config{};
    nx_stm32_ltdc_layer_t layers[2]{};
    nx_stm32_ltdc_pin_t* pins = nullptr;
    uint16_t* snapshots = nullptr;
    nx_stm32_ltdc_state_t state{};
    nx_display_request_t first{}, second{}, third{};
    nx_display_port_t port{};

    void SetUp() override {
        hardware = &model;
        void* mapped = mmap(nullptr, 12288, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        ASSERT_NE(mapped, MAP_FAILED);
        memory = static_cast<uint8_t*>(mapped);
        system = reinterpret_cast<nx_stm32_system_t*>(memory + 512);
        system->rcc = &model.rcc;
        system->flash = FLASH;
        system->started = true;
        system->core_hz = 16000000;
        system->time_epoch = 1;
        region = {reinterpret_cast<uintptr_t>(memory), 12288,
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        model.rcc.CR =
            RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY;
        model.rcc.PLLCFGR = 16;
        model.rcc.PLLSAICFGR = (192U << RCC_PLLSAICFGR_PLLSAIN_Pos) |
                               (4U << RCC_PLLSAICFGR_PLLSAIR_Pos);
        model.rcc.DCKCFGR = RCC_DCKCFGR_PLLSAIDIVR;
        model.rcc.APB2ENR = 0;
        config.registers = &model.registers;
        config.rcc = &model.rcc;
        config.regions = &region;
        config.region_count = 1;
        config.system = system;
        config.pll_source = 16;
        config.pllsai_cfgr = model.rcc.PLLSAICFGR;
        config.pllsai_divr = model.rcc.DCKCFGR;
        config.timing.HorizontalSync = 1;
        config.timing.VerticalSync = 1;
        config.timing.AccumulatedHBP = 2;
        config.timing.AccumulatedVBP = 2;
        config.timing.AccumulatedActiveW = 10;
        config.timing.AccumulatedActiveH = 6;
        config.timing.TotalWidth = 13;
        config.timing.TotalHeigh = 8;
        layers[0] = {{8, 4, 4, 4, 8191, 3, 0}, 0, 0, 255, false};
        layers[1] = {{8, 4, 4, 4, 8191, 3, 1}, 0, 0, 255, false};
        /* The parent owns these original snapshots, not the LTDC driver. */
        pins = reinterpret_cast<nx_stm32_ltdc_pin_t*>(memory + 1024);
        snapshots = reinterpret_cast<uint16_t*>(memory + 1536);
        static const uint8_t route[20][2] = {
            {7, 9},  {7, 10}, {7, 11}, {7, 12}, {6, 6},  {7, 13}, {7, 14},
            {7, 15}, {8, 0},  {8, 1},  {8, 2},  {6, 11}, {8, 4},  {8, 5},
            {8, 6},  {8, 7},  {8, 12}, {8, 13}, {8, 14}, {5, 10}};
        for (size_t i = 0; i < 20; ++i) {
            pins[i] = {&model.gpio[route[i][0]], route[i][0], route[i][1]};
            snapshots[i] = 0x59a5;
        }
        model.rcc.AHB1ENR = UINT32_C(0x1ff);
        model.gpio[0].MODER = 2U << 6U;
        model.gpio[0].AFR[0] = 7U << 12U;
        config.pins = pins;
        config.pin_count = 20;
        config.snapshots = snapshots;
        config.layers = layers;
        config.layer_count = 1;
        config.irq_policy = {NX_IRQ_KERNEL_NONE, 93, 4, 0};
        config.priority = 5;
        config.drain_us = 100;
        model.priority[LTDC_IRQn] = 6;
        model.priority[LTDC_ER_IRQn] = 7;
        port = {&nx_stm32_ltdc_ops, &state};
        nx_request_initialize(&first.base);
        nx_request_initialize(&second.base);
        nx_request_initialize(&third.base);
    }
    void TearDown() override {
        if (memory != nullptr) {
            EXPECT_EQ(munmap(memory, 12288), 0);
        }
        hardware = nullptr;
    }
    void prepare(nx_display_request_t* request, unsigned slot,
                 unsigned layer = 0) {
        nx_display_operation_t op{};
        const auto& limits = layers[layer].limits;
        const size_t row = limits.width * 2U;
        op.frame = {memory + slot * 4096, row, row, limits.height, 256};
        op.format = NX_IMAGE_RGB565;
        op.layer = static_cast<uint8_t>(layer);
        ASSERT_EQ(nx_display_prepare(request, &op, NX_DEADLINE_NEVER),
                  NX_SUCCESS);
    }
    void open() {
        ASSERT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_SUCCESS);
    }
    nx_display_port_t peer() {
        return port;
    }
    void open_both() {
        config.layer_count = 2;
        open();
    }
    void show_both() {
        show_first();
        auto other = peer();
        prepare(&second, 1, 1);
        ASSERT_EQ(nx_display_port_submit(&other, &second), NX_SUCCESS);
        ASSERT_EQ(nx_display_port_service(&other), NX_SUCCESS);
        vblank();
        irq();
        ASSERT_EQ(nx_display_port_service(&other), NX_SUCCESS);
    }
    void vblank(bool in_blank = true) {
        if (!model.hold_reload && model.registers.SRCR != 0U &&
            (model.registers.GCR & LTDC_GCR_LTDCEN) != 0U) {
            std::memcpy(model.active, model.shadow, sizeof(model.active));
            model.registers.SRCR = 0;
            model.registers.ISR |= LTDC_ISR_RRIF;
        }
        model.registers.CDSR = in_blank ? LTDC_CDSR_VSYNCS : 0U;
        if (in_blank && (model.registers.GCR & LTDC_GCR_LTDCEN) != 0U &&
            (model.registers.IER & LTDC_IT_LI) != 0U &&
            model.registers.LIPCR == 0U)
            model.registers.ISR |= LTDC_ISR_LIF;
    }
    void irq() {
        model.in_isr = true;
        model.in_irq[LTDC_IRQn] = 1;
        nx_stm32_ltdc_irq(&state);
        model.in_irq[LTDC_IRQn] = 0;
        model.in_isr = false;
    }
    void show_first() {
        prepare(&first, 0);
        ASSERT_EQ(nx_display_port_submit(&port, &first), NX_SUCCESS);
        ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
        vblank();
        irq();
        ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    }
    nx_result_t finish_stop() {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned i = 0; i < 8 && result != NX_SUCCESS; ++i) {
            result = nx_display_port_stop(&port);
            vblank();
            if (model.enabled[LTDC_IRQn] != 0U)
                irq();
        }
        return result;
    }
};

TEST_F(LtdcOwner, PlatformAdmissionRunsBeforeSystemOrGPIOPowerWithoutEffects) {
    system->started = false;
    system->time_epoch = 0;
    model.rcc.CR = 0;
    model.rcc.AHB1ENR = 0;
    const auto before = state;
    EXPECT_EQ(nx_stm32_ltdc_admit(&state, &config), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(&before, &state, sizeof(state)), 0);
    EXPECT_EQ(model.writes, 0U);
}

TEST_F(LtdcOwner, PlatformAdmissionRefusesForeignGateAndBothVectorDomains) {
    model.rcc.APB2ENR = RCC_APB2ENR_LTDCEN;
    EXPECT_EQ(nx_stm32_ltdc_admit(&state, &config), NX_ERROR_BUSY);
    model.rcc.APB2ENR = 0;
    model.pending[LTDC_IRQn] = 1;
    EXPECT_EQ(nx_stm32_ltdc_admit(&state, &config), NX_ERROR_BUSY);
    model.pending[LTDC_IRQn] = 0;
    model.in_irq[LTDC_ER_IRQn] = 1;
    EXPECT_EQ(nx_stm32_ltdc_admit(&state, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(model.writes, 0U);
}

TEST_F(LtdcOwner, ColdForeignIRQIsRejectedBeforeAnyControllerEffect) {
    model.enabled[LTDC_ER_IRQn] = 1;
    const auto before = model.registers;
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(std::memcmp(&before, &model.registers, sizeof(before)), 0);
    EXPECT_EQ(model.writes, 0U);
}

TEST_F(LtdcOwner, ColdOwnerAcquiresAndReturnsOnlyItsControllerGate) {
    model.rcc.APB2ENR = RCC_APB2ENR_SYSCFGEN;
    open();
    ASSERT_TRUE(state.gate_owned);
    EXPECT_EQ(model.rcc.APB2ENR,
              static_cast<uint32_t>(RCC_APB2ENR_SYSCFGEN | RCC_APB2ENR_LTDCEN));
    ASSERT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_FALSE(state.gate_owned);
    EXPECT_EQ(model.rcc.APB2ENR, static_cast<uint32_t>(RCC_APB2ENR_SYSCFGEN));
}

TEST_F(LtdcOwner, CpuOnlySdramAddressDoesNotGrantAScanoutLoan) {
    open();
    prepare(&first, 0U);
    first.operation.frame.data =
        reinterpret_cast<const uint8_t*>(UINT32_C(0xc0000000));
    const auto before = first;
    const auto registers = model.registers;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_display_port_submit(&port, &first));
    EXPECT_EQ(0, std::memcmp(&before, &first, sizeof(first)));
    EXPECT_EQ(0, std::memcmp(&registers, &model.registers, sizeof(registers)));
    EXPECT_EQ(nullptr, state.pending);
}

TEST_F(LtdcOwner, APreownedControllerGateIsRefusedBeforeColdEffects) {
    model.rcc.APB2ENR |= RCC_APB2ENR_LTDCEN;
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(model.writes, 0U);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(LtdcOwner, ARefusedGateEnableKeepsARetryableColdReceipt) {
    model.rcc.APB2ENR = 0;
    model.refuse_gate_enable = true;
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.owned);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(model.registers.GCR, 0U);
    model.refuse_gate_enable = false;
    EXPECT_EQ(nx_stm32_ltdc_close(&state), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_FALSE(state.gate_owned);
}

TEST_F(LtdcOwner, ARefusedGateDisableRetainsColdConfigAndDoesNotReenableIt) {
    model.rcc.APB2ENR = 0;
    open();
    model.refuse_gate_disable = true;
    EXPECT_NE(finish_stop(), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(state.config, &config);
    EXPECT_TRUE(state.gate_owned);
    model.refuse_gate_disable = false;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    const auto writes = model.writes;
    config.registers = nullptr;
    EXPECT_EQ(nx_stm32_ltdc_close(&state), NX_SUCCESS);
    EXPECT_EQ(model.writes, writes);
}

TEST_F(LtdcOwner, HALPadsUseAF14AndCloseOnlyWithdrawsTheirParentLeases) {
    open();
    for (size_t i = 0; i < 20; ++i) {
        const auto& p = pins[i];
        EXPECT_EQ((p.registers->MODER >> (2U * p.pin)) & 3U, 2U);
        EXPECT_EQ((p.registers->AFR[p.pin / 8U] >> (4U * (p.pin % 8U))) & 15U,
                  14U);
        EXPECT_EQ((p.registers->OSPEEDR >> (2U * p.pin)) & 3U, 3U);
        EXPECT_EQ((p.registers->PUPDR >> (2U * p.pin)) & 3U, 0U);
        EXPECT_EQ((p.registers->OTYPER >> p.pin) & 1U, 0U);
        EXPECT_EQ(snapshots[i], 0x59a5);
    }
    EXPECT_NE(state.pins_owned, 0U);
    ASSERT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(state.pins_owned, 0U);
    for (size_t i = 0; i < 20; ++i) {
        EXPECT_EQ((pins[i].registers->MODER >> (2U * pins[i].pin)) & 3U, 3U);
        EXPECT_EQ(snapshots[i], 0x59a5);
    }
    EXPECT_EQ(model.rcc.AHB1ENR & 0x1ffU, 0x1ffU);
    EXPECT_EQ(model.gpio[0].MODER, 2U << 6U);
    EXPECT_EQ(model.gpio[0].AFR[0], 7U << 12U);
}

TEST_F(LtdcOwner, MalformedOrUnpoweredPadsRejectBeforeAnyColdEffect) {
    pins[1] = pins[0];
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(model.writes, 0U);
    EXPECT_FALSE(state.owned);
    pins[1] = {&model.gpio[7], 7, 10};
    config.snapshots = reinterpret_cast<const uint16_t*>(layers);
    EXPECT_EQ(nx_stm32_ltdc_admit(&state, &config), NX_ERROR_INVALID);
    config.snapshots = reinterpret_cast<const uint16_t*>(&region);
    EXPECT_EQ(nx_stm32_ltdc_admit(&state, &config), NX_ERROR_INVALID);
    config.snapshots = snapshots;
    model.rcc.AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_GPIOHEN);
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(model.writes, 0U);
    EXPECT_FALSE(state.owned);
}

TEST_F(LtdcOwner, RefusedHALPadReadbackRetainsChangedPinsForExplicitClose) {
    model.refuse_pad_setup = true;
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.owned);
    EXPECT_NE(state.pins_owned, 0U);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_EQ(state.config, &config);
    model.refuse_pad_setup = false;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(state.pins_owned, 0U);
}

TEST_F(LtdcOwner, RefusedPadWithdrawalRetainsControllerGateAndColdConfig) {
    open();
    model.refuse_pad_release = true;
    EXPECT_NE(finish_stop(), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_NE(state.pins_owned, 0U);
    EXPECT_NE(model.rcc.APB2ENR & RCC_APB2ENR_LTDCEN, 0U);
    EXPECT_EQ(state.config, &config);
    model.refuse_pad_release = false;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(model.rcc.APB2ENR & RCC_APB2ENR_LTDCEN, 0U);
}

TEST_F(LtdcOwner, CompletePadAndSnapshotMetadataNeverBecomeFrameOrQueryOutput) {
    open();
    prepare(&first, 0);
    first.operation.frame.data = reinterpret_cast<const uint8_t*>(snapshots);
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
    first.operation.frame.data = reinterpret_cast<const uint8_t*>(pins);
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
    auto* out = reinterpret_cast<nx_display_constraints_t*>(snapshots);
    EXPECT_EQ(nx_display_port_query(&port, 0, out), NX_ERROR_INVALID);
    for (size_t i = 0; i < 20; ++i)
        EXPECT_EQ(snapshots[i], 0x59a5);
}

TEST_F(LtdcOwner, QuerySelectsOneLayerAndDoesNotStartOrBorrowAFrame) {
    open();
    nx_display_constraints_t limits{};
    ASSERT_EQ(nx_display_port_query(&port, 0, &limits), NX_SUCCESS);
    EXPECT_EQ(limits.layer, 0U);
    EXPECT_EQ(limits.formats, 3U);
    EXPECT_EQ(limits.width, 8U);
    EXPECT_EQ(limits.height, 4U);
    EXPECT_EQ(model.active[0].CR & LTDC_LxCR_LEN, 0U);
    EXPECT_FALSE(state.active[0]);
    EXPECT_FALSE(state.pending);
}

TEST_F(LtdcOwner, InitialVisibilityDoesNotSettleTheContinuousFrameLoan) {
    open();
    show_first();
    nx_display_snapshot_t snapshot{};
    ASSERT_EQ(nx_display_port_snapshot(&port, 0, &snapshot), NX_SUCCESS);
    EXPECT_TRUE(snapshot.active);
    EXPECT_TRUE(snapshot.visible);
    EXPECT_FALSE(snapshot.pending);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.active[0].CFBAR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(memory)));
    EXPECT_EQ(model.active[0].CFBLR, (16U << 16U) | 19U);
}

TEST_F(LtdcOwner, PendingAndItsReloadHintCannotReturnTheOldFrameEarly) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    /* A stale/standalone reload event has no active-register proof. */
    model.registers.ISR |= LTDC_ISR_RRIF;
    irq();
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    nx_display_snapshot_t snapshot{};
    ASSERT_EQ(nx_display_port_snapshot(&port, 0, &snapshot), NX_SUCCESS);
    EXPECT_TRUE(snapshot.visible); /* Current active A, never pending B. */
    EXPECT_TRUE(snapshot.pending);
}

TEST_F(LtdcOwner, ActiveReadbackWithoutObservedBlankingStillRetainsBothFrames) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank(false);
    irq();
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    model.registers.CDSR = LTDC_CDSR_VSYNCS;
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(first.base.result, NX_SUCCESS);
    EXPECT_EQ(first.base.transferred, 0U);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner,
       ReplacementRequiresTheMatchingActiveAddressNotJustVBRAndVSYNC) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    model.registers.SRCR = 0;
    model.registers.CDSR = LTDC_CDSR_VSYNCS;
    EXPECT_NE(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, OnePendingSlotAndAnUnsupportedLayerRejectWithoutEffects) {
    open();
    show_first();
    prepare(&second, 1);
    prepare(&third, 2);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    const auto before = third;
    const auto shadow_before = model.shadow[0];
    EXPECT_EQ(nx_display_port_submit(&port, &third), NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&third, &before, sizeof(third)), 0);
    EXPECT_EQ(
        std::memcmp(&shadow_before, &model.shadow[0], sizeof(shadow_before)),
        0);
}

TEST_F(LtdcOwner, SelectedLayerIsNotAnImplicitSecondControllerOwner) {
    open();
    prepare(&first, 0);
    first.operation.layer = 1;
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
    EXPECT_EQ(model.registers.SRCR, 0U);
}

TEST_F(LtdcOwner, AllPeerRequestAndFrameSpansAreMutuallyIndependent) {
    open();
    auto* inside = reinterpret_cast<nx_display_request_t*>(memory + 64);
    nx_request_initialize(&inside->base);
    prepare(inside, 1);
    show_first();
    EXPECT_EQ(nx_display_port_submit(&port, inside), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&inside->base), NX_REQUEST_READY);
    prepare(&second, 1);
    second.operation.frame.data = reinterpret_cast<const uint8_t*>(&first);
    EXPECT_EQ(nx_display_port_submit(&port, &second), NX_ERROR_INVALID);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, CompleteStateAndResourceFactsAreExcludedBeforeDMAAdmission) {
    open();
    prepare(&first, 0);
    first.operation.frame.data = reinterpret_cast<const uint8_t*>(&state);
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_INVALID);
    first.operation.frame.data = reinterpret_cast<const uint8_t*>(&config);
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_INVALID);
    first.operation.frame.data = reinterpret_cast<const uint8_t*>(&region);
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
}

TEST_F(LtdcOwner,
       GracefulStopWaitsForDisableReloadThenReturnsPrioritiesAndLoans) {
    open();
    show_first();
    EXPECT_EQ(nx_display_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(first.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(model.active[0].CR & LTDC_LxCR_LEN, 0U);
    EXPECT_EQ(model.active[0].CFBAR, 0U);
    EXPECT_EQ(model.registers.GCR & LTDC_GCR_LTDCEN, 0U);
    EXPECT_EQ(model.enabled[LTDC_IRQn], 0U);
    EXPECT_EQ(model.enabled[LTDC_ER_IRQn], 0U);
    EXPECT_EQ(model.priority[LTDC_IRQn], 6U);
    EXPECT_EQ(model.priority[LTDC_ER_IRQn], 7U);
    EXPECT_EQ(state.config, nullptr);
    EXPECT_EQ(nx_display_port_stop(&port), NX_SUCCESS);
}

TEST_F(LtdcOwner,
       RefusedControllerDisableRetainsTheFrameAndColdReceiptForRetry) {
    open();
    show_first();
    model.hold_disable = true;
    EXPECT_NE(finish_stop(), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(state.config, &config);
    model.hold_disable = false;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, OutstandingIRQOrWakePublisherPreventsConfigurationReturn) {
    open();
    show_first();
    model.hold_irq = true;
    EXPECT_NE(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(state.config, &config);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    model.hold_irq = false;
    state.publishers = 1;
    EXPECT_NE(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(state.config, &config);
    state.publishers = 0;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
}

TEST_F(LtdcOwner, AClockLossDoesNotBecomeAnUnprovedResetBasedBufferReturn) {
    open();
    show_first();
    model.rcc.CR &= ~static_cast<uint32_t>(RCC_CR_PLLSAIRDY);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(nx_display_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(model.rcc.APB2RSTR & RCC_APB2RSTR_LTDCRST, 0U);
}

TEST_F(LtdcOwner,
       HardwareBusErrorWinsOverSimultaneousReloadAndRetainsBothLoans) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    model.registers.ISR |= LTDC_ISR_TERRIF;
    irq();
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(nx_display_port_stop(&port), NX_SUCCESS);
}
TEST_F(LtdcOwner, QueryCannotOverwriteAnAlreadyBorrowedFrameOrRequest) {
    open();
    show_first();
    std::array<uint8_t, sizeof(nx_display_constraints_t)> before{};
    std::memcpy(before.data(), memory, before.size());
    auto* output = reinterpret_cast<nx_display_constraints_t*>(memory);
    EXPECT_EQ(nx_display_port_query(&port, 0, output), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(before.data(), memory, before.size()), 0);
    EXPECT_EQ(
        nx_display_port_query(
            &port, 0, reinterpret_cast<nx_display_constraints_t*>(&first)),
        NX_ERROR_INVALID);
}

TEST_F(LtdcOwner,
       CancelBeforePendingHardwareEffectDoesNotRetireTheActiveFrame) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_cancel(&port, &second), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(second.base.result, NX_ERROR_CANCELLED);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.registers.SRCR, 0U);
    EXPECT_NE(model.active[0].CR & LTDC_LxCR_LEN, 0U);
    nx_display_snapshot_t value{};
    ASSERT_EQ(nx_display_port_snapshot(&port, 0, &value), NX_SUCCESS);
    EXPECT_TRUE(value.active);
    EXPECT_TRUE(value.visible);
    EXPECT_FALSE(value.pending);
}

TEST_F(LtdcOwner, PendingDeadlineBeforeFirstEffectDoesNotBlankTheActiveFrame) {
    open();
    show_first();
    prepare(&second, 1);
    second.base.deadline = 2;
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    model.now = 3;
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(second.base.result, NX_ERROR_TIMEOUT);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.registers.SRCR, 0U);
    EXPECT_NE(model.active[0].CR & LTDC_LxCR_LEN, 0U);
}

TEST_F(LtdcOwner, SuccessfulCloseAllowsAFreshSessionOnTheSameStaticState) {
    open();
    show_first();
    ASSERT_EQ(finish_stop(), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_SUCCESS);
    prepare(&first, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &first), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    irq();
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.first_failure, NX_SUCCESS);
}

TEST_F(LtdcOwner, ActiveCancelThenReuseRestoresCompletionAndErrorPublishers) {
    open();
    show_first();
    ASSERT_EQ(nx_display_port_cancel(&port, &first), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    vblank();
    if (model.enabled[LTDC_IRQn] != 0U)
        irq();
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    ASSERT_TRUE(state.owned);
    ASSERT_TRUE(state.initialized);
    unsigned hints = 0;
    nx_irq_wake_t wake{&hints,
                       [](void* context) {
                           ++*static_cast<unsigned*>(context);
                           return NX_SUCCESS;
                       },
                       false};
    ASSERT_EQ(nx_display_port_attach_wake(&port, &wake, 0), NX_SUCCESS);
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(model.enabled[LTDC_IRQn], 1U);
    EXPECT_EQ(model.enabled[LTDC_ER_IRQn], 1U);
    EXPECT_EQ(model.registers.IER & (LTDC_IT_TE | LTDC_IT_FU),
              LTDC_IT_TE | LTDC_IT_FU);
    vblank();
    if (model.enabled[LTDC_IRQn] != 0U)
        irq();
    EXPECT_GT(hints, 0U);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    const auto reload_hints = hints;
    model.registers.ISR |= LTDC_ISR_FUIF;
    if (model.enabled[LTDC_ER_IRQn] != 0U)
        irq();
    EXPECT_GT(hints, reload_hints);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_QUARANTINED);
}

TEST_F(LtdcOwner, ProductionAdmissionExcludesTheWholeConcreteSystemOwner) {
    open();
    prepare(&first, 0);
    first.operation.frame.data =
        reinterpret_cast<const uint8_t*>(&system->reset_cause);
    const auto writes = model.writes;
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
    EXPECT_EQ(model.writes, writes);
}

TEST_F(LtdcOwner, OneControllerQueriesTwoExplicitLayerIdentities) {
    open_both();
    auto other = peer();
    nx_display_constraints_t a{}, b{};
    ASSERT_EQ(nx_display_port_query(&port, 0, &a), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_query(&other, 1, &b), NX_SUCCESS);
    EXPECT_EQ(a.layer, 0U);
    EXPECT_EQ(b.layer, 1U);
    EXPECT_EQ(port.context, &state);
    EXPECT_EQ(other.context, &state);
    EXPECT_EQ(state.config, &config);
}

TEST_F(LtdcOwner, EachLayerUsesItsExplicitWindowAndAlphaWithoutAnotherHandle) {
    layers[1].limits.width = 4;
    layers[1].limits.height = 2;
    layers[1].x = 2;
    layers[1].y = 1;
    layers[1].alpha = 128;
    layers[1].pixel_alpha = true;
    open_both();
    show_both();
    EXPECT_EQ(state.active[0], &first);
    EXPECT_EQ(state.active[1], &second);
    EXPECT_EQ(model.active[1].WHPCR, 5U | (8U << 16U));
    EXPECT_EQ(model.active[1].WVPCR, 4U | (5U << 16U));
    EXPECT_EQ(model.active[1].CACR, 128U);
    EXPECT_EQ(model.active[1].BFCR,
              LTDC_BLENDING_FACTOR1_PAxCA | LTDC_BLENDING_FACTOR2_PAxCA);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, CrossLayerFrameLoansRemainMutuallyDisjoint) {
    open_both();
    show_both();
    prepare(&third, 1, 0);
    const auto writes = model.writes;
    EXPECT_EQ(nx_display_port_submit(&port, &third), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&third.base), NX_REQUEST_READY);
    EXPECT_EQ(model.writes, writes);
}

TEST_F(LtdcOwner, OneGlobalPendingCommitCannotBeOverwrittenByTheOtherLayer) {
    open_both();
    show_both();
    prepare(&third, 2, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &third), NX_SUCCESS);
    nx_display_request_t fourth{};
    nx_request_initialize(&fourth.base);
    prepare(&fourth, 2, 1);
    auto other = peer();
    const auto before = fourth;
    const auto writes = model.writes;
    EXPECT_EQ(nx_display_port_submit(&other, &fourth), NX_ERROR_BUSY);
    EXPECT_EQ(std::memcmp(&before, &fourth, sizeof(fourth)), 0);
    EXPECT_EQ(model.writes, writes);
}

TEST_F(LtdcOwner, ReplacementRetiresOnlyItsOldLayerAndKeepsPeerBorrowed) {
    open_both();
    show_both();
    prepare(&third, 2, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &third), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    irq();
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(first.base.result, NX_SUCCESS);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&third.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active[0], &third);
    EXPECT_EQ(state.active[1], &second);
}

TEST_F(LtdcOwner, SelectedCancelPreservesPeerScanoutAndGlobalErrorPublishers) {
    open_both();
    show_both();
    ASSERT_EQ(nx_display_port_cancel(&port, &first), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    vblank();
    irq();
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(first.base.result, NX_ERROR_CANCELLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.active[0].CR & LTDC_LxCR_LEN, 0U);
    EXPECT_NE(model.active[1].CR & LTDC_LxCR_LEN, 0U);
    EXPECT_NE(model.registers.GCR & LTDC_GCR_LTDCEN, 0U);
    EXPECT_EQ(model.enabled[LTDC_ER_IRQn], 1U);
    EXPECT_EQ(model.registers.IER & (LTDC_IT_TE | LTDC_IT_FU),
              LTDC_IT_TE | LTDC_IT_FU);
}

TEST_F(LtdcOwner, SelectedRetireRequiresTheUnchangedPeerAddressProof) {
    open_both();
    show_both();
    ASSERT_EQ(nx_display_port_cancel(&port, &first), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    vblank();
    irq();
    const auto peer_address = model.active[1].CFBAR;
    model.active[1].CFBAR ^= 4U;
    EXPECT_NE(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    model.active[1].CFBAR = peer_address;
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner,
       ControllerStopKeepsBothLayerLoansOnFailedReloadThenJoinsBoth) {
    open_both();
    show_both();
    model.hold_reload = true;
    auto other = peer();
    EXPECT_NE(nx_display_port_stop(&other), NX_SUCCESS);
    EXPECT_NE(finish_stop(), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.config, &config);
    model.hold_reload = false;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_FALSE(state.owned);
}

TEST_F(LtdcOwner, DuplicateHardwareLayerFactsRejectBeforeAnyEffect) {
    layers[1].limits.layer = 0;
    config.layer_count = 2;
    const auto writes = model.writes;
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_INVALID);
    EXPECT_EQ(model.writes, writes);
    EXPECT_FALSE(state.owned);
}

TEST_F(LtdcOwner,
       CancelActiveKeepsAnUnstagedReplacementBorrowedForLaterDisplay) {
    open_both();
    show_both();
    prepare(&third, 2, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &third), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_cancel(&port, &first), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    vblank();
    irq();
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&third.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.pending, &third);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    irq();
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(state.active[0], &third);
    EXPECT_NE(nx_request_state(&third.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, PeerCancelHasAFiniteBudgetBehindAStuckGlobalCommit) {
    open_both();
    show_both();
    prepare(&third, 2, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &third), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    model.hold_reload = true;
    auto other = peer();
    ASSERT_EQ(nx_display_port_cancel(&other, &second), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&other), NX_ERROR_BUSY);
    model.now = 200;
    EXPECT_EQ(nx_display_port_service(&other), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_state(&third.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active[1], &second);
    EXPECT_EQ(state.pending, &third);
}

TEST_F(LtdcOwner, StopBeforeAnyFrameEffectDoesNotWaitForADisabledTimingEngine) {
    model.refuse_enable = true;
    open();
    prepare(&first, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &first), NX_SUCCESS);
    ASSERT_EQ(model.registers.GCR & LTDC_GCR_LTDCEN, 0U);
    EXPECT_EQ(nx_display_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(first.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(model.registers.SRCR, 0U);
}

TEST_F(LtdcOwner, DroppingUnstagedPendingDuringStopDoesNotReturnLivePeerLoans) {
    open_both();
    show_both();
    prepare(&third, 2, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &third), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&third.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, PreFramePublisherFailureCanJoinWithoutInventingAFrameReload) {
    model.refuse_enable = true;
    open();
    prepare(&first, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &first), NX_SUCCESS);
    model.hold_irq = true;
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(model.registers.GCR & LTDC_GCR_LTDCEN, 0U);
    model.hold_irq = false;
    EXPECT_EQ(nx_display_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(first.base.result, NX_ERROR_IO);
}

TEST_F(LtdcOwner, IrqBlankingProofAllowsLateTaskServiceOutsideThePulse) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    irq();
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    model.registers.CDSR = 0;
    model.now = 5000;
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, MissedReloadPulseUsesABoundedLineInterruptProofRetry) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank(false);
    irq();
    EXPECT_NE(model.registers.IER & LTDC_IT_LI, 0U);
    EXPECT_EQ(model.registers.LIPCR, 0U);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    model.registers.CDSR = LTDC_CDSR_VSYNCS;
    model.registers.ISR |= LTDC_ISR_LIF;
    irq();
    model.registers.CDSR = 0;
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(model.registers.IER & (LTDC_IT_RR | LTDC_IT_LI), 0U);
}

TEST_F(LtdcOwner, OldReloadCannotProveAReusedRequestInANewGeneration) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    irq();
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    const auto previous = state.stage_generation;
    prepare(&first, 0);
    ASSERT_EQ(nx_display_port_submit(&port, &first), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_GT(state.stage_generation, previous);
    model.registers.ISR |= LTDC_ISR_RRIF | LTDC_ISR_LIF;
    irq();
    model.registers.CDSR = 0;
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    vblank();
    irq();
    model.registers.CDSR = 0;
    EXPECT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, ClockEpochChangeInvalidatesAnAlreadyCapturedIrqProof) {
    open();
    show_first();
    prepare(&second, 1);
    ASSERT_EQ(nx_display_port_submit(&port, &second), NX_SUCCESS);
    ASSERT_EQ(nx_display_port_service(&port), NX_SUCCESS);
    vblank();
    irq();
    ++system->time_epoch;
    model.registers.CDSR = 0;
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_QUARANTINED);
    EXPECT_TRUE(state.owned);
}

TEST_F(LtdcOwner, RetireProofCannotReturnAPeerAddedToControllerStop) {
    open_both();
    show_both();
    ASSERT_EQ(nx_display_port_cancel(&port, &first), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_service(&port), NX_ERROR_BUSY);
    vblank();
    irq();
    model.registers.CDSR = 0;
    EXPECT_EQ(nx_display_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_SETTLED);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_display_port_stop(&port), NX_ERROR_BUSY);
    model.registers.ISR |= LTDC_ISR_RRIF;
    irq();
    EXPECT_EQ(nx_display_port_stop(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&second.base), NX_REQUEST_SETTLED);
    vblank();
    irq();
    model.registers.CDSR = 0;
    EXPECT_EQ(nx_display_port_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&second.base), NX_REQUEST_SETTLED);
}

TEST_F(LtdcOwner, GenerationExhaustionRejectsBeforeAnyBorrowOrEffect) {
    open();
    state.stage_generation = UINT64_MAX;
    prepare(&first, 0);
    const auto writes = model.writes;
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
    EXPECT_EQ(model.writes, writes);
    EXPECT_EQ(state.pending, nullptr);
}

TEST_F(LtdcOwner, GraphicsAttachmentRefusalPrecedesAnyControllerEffect) {
    config.graphics_memory = &graphics_binding;
    model.attach_result = NX_ERROR_STATE;
    const auto writes = model.writes;
    EXPECT_EQ(nx_stm32_ltdc_prepare(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(model.graphics_attaches, 1U);
    EXPECT_EQ(model.writes, writes);
    EXPECT_EQ(model.rcc.APB2ENR & RCC_APB2ENR_LTDCEN, 0U);
    EXPECT_EQ(nx_stm32_ltdc_close(&state), NX_SUCCESS);
    EXPECT_EQ(model.graphics_detaches, 0U);
}
TEST_F(LtdcOwner, GraphicsHoldSurvivesFailedIrqJoinUntilActualColdClose) {
    config.graphics_memory = &graphics_binding;
    open();
    ASSERT_EQ(model.graphics_attaches, 1U);
    model.hold_irq = true;
    EXPECT_EQ(nx_stm32_ltdc_close(&state), NX_ERROR_BUSY);
    EXPECT_EQ(model.graphics_detaches, 0U);
    EXPECT_TRUE(state.memory_attached);
    model.hold_irq = false;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(model.graphics_detaches, 1U);
    EXPECT_FALSE(state.memory_attached);
}
TEST_F(LtdcOwner,
       GraphicsDetachFailureRetainsOriginalOwnerAndCanRetryAfterGateOff) {
    config.graphics_memory = &graphics_binding;
    open();
    model.detach_result = NX_ERROR_STATE;
    EXPECT_EQ(nx_stm32_ltdc_close(&state), NX_ERROR_BUSY);
    vblank();
    irq();
    EXPECT_EQ(nx_stm32_ltdc_close(&state), NX_ERROR_STATE);
    EXPECT_TRUE(state.owned);
    EXPECT_TRUE(state.memory_attached);
    EXPECT_EQ(model.rcc.APB2ENR & RCC_APB2ENR_LTDCEN, 0U);
    model.detach_result = NX_SUCCESS;
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
    EXPECT_EQ(model.graphics_detaches, 2U);
    EXPECT_FALSE(state.owned);
}
TEST_F(LtdcOwner, LostGraphicsBankEvidenceRejectsBeforeRequestBorrow) {
    config.graphics_memory = &graphics_binding;
    open();
    prepare(&first, 0);
    model.check_result = NX_ERROR_STATE;
    const auto writes = model.writes;
    EXPECT_EQ(nx_display_port_submit(&port, &first), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&first.base), NX_REQUEST_READY);
    EXPECT_EQ(model.writes, writes);
    EXPECT_EQ(model.graphics_checks, 1U);
    EXPECT_EQ(finish_stop(), NX_SUCCESS);
}

TEST_F(LtdcOwner, QueryCannotOverwriteTheRetainedBankOwnerMetadata) {
    config.graphics_memory = &graphics_binding;
    open();
    auto* output =
        reinterpret_cast<nx_display_constraints_t*>(model.bank_metadata);
    const auto before = model;
    EXPECT_EQ(nx_display_port_query(&port, 0, output), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(model.bank_metadata, before.bank_metadata,
                          sizeof(model.bank_metadata)),
              0);
}

} /* namespace */

extern "C" uint32_t nx_gfx_model_read(volatile uint32_t* reg) {
    if (reg == &hardware->gpio[8].AFR[1] && hardware->refuse_pad_setup)
        return 0U;
    if (reg == &hardware->registers.GCR && hardware->hold_disable)
        *reg |= LTDC_GCR_LTDCEN;
    return *reg;
}
extern "C" void nx_gfx_model_write(volatile uint32_t* reg, uint32_t value) {
    ++hardware->writes;
    if (reg == &hardware->gpio[8].MODER && hardware->refuse_pad_release &&
        ((value >> 24U) & 3U) == 3U)
        return;
    if (reg == &hardware->rcc.APB2ENR &&
        (((value & RCC_APB2ENR_LTDCEN) != 0U && hardware->refuse_gate_enable) ||
         ((value & RCC_APB2ENR_LTDCEN) == 0U && hardware->refuse_gate_disable)))
        return;
    if (reg == &hardware->registers.GCR && hardware->refuse_enable)
        value &= ~static_cast<uint32_t>(LTDC_GCR_LTDCEN);
    if (reg == &hardware->registers.ICR) {
        hardware->registers.ISR &= ~value;
        *reg = value;
    } else {
        *reg = value;
    }
}
extern "C" LTDC_Layer_TypeDef* nx_gfx_model_shadow(LTDC_HandleTypeDef*,
                                                   uint32_t layer) {
    return &hardware->shadow[layer];
}
extern "C" LTDC_Layer_TypeDef* nx_gfx_model_active_layer(LTDC_HandleTypeDef*,
                                                         uint32_t layer) {
    return &hardware->active[layer];
}
extern "C" LTDC_TypeDef* nx_gfx_model_registers(void) {
    return &hardware->registers;
}
extern "C" nx_time_us_t nx_gfx_model_now(void) {
    return hardware->now;
}
extern "C" bool nx_gfx_model_task(void) {
    return hardware->privileged && !hardware->in_isr;
}
extern "C" uint32_t nx_gfx_model_irq_enabled(IRQn_Type irq) {
    return hardware->enabled[irq];
}
extern "C" uint32_t nx_gfx_model_irq_pending(IRQn_Type irq) {
    return hardware->pending[irq];
}
extern "C" uint32_t nx_gfx_model_irq_active(IRQn_Type irq) {
    return hardware->hold_irq ? 1U : hardware->in_irq[irq];
}
extern "C" void nx_gfx_model_irq_enable(IRQn_Type irq) {
    hardware->enabled[irq] = 1;
}
extern "C" void nx_gfx_model_irq_disable(IRQn_Type irq) {
    hardware->enabled[irq] = 0;
}
extern "C" void nx_gfx_model_irq_clear(IRQn_Type irq) {
    hardware->pending[irq] = 0;
}
extern "C" uint32_t nx_gfx_model_irq_priority(IRQn_Type irq) {
    return hardware->priority[irq];
}
extern "C" void nx_gfx_model_irq_set_priority(IRQn_Type irq, uint32_t value) {
    if (!hardware->hold_priority)
        hardware->priority[irq] = value;
}

/* The SDRAM owner is an external lifetime dependency in this engine model.
 * Its original-lease rules are tested by the real SDRAM provider program. */
extern "C" nx_result_t
nx_stm32_sdram_graphics_attach(nx_stm32_sdram_graphics_t*,
                               nx_stm32_sdram_graphics_master_t master,
                               const void* consumer, const void* system) {
    EXPECT_EQ(master, NX_STM32_SDRAM_GRAPHICS_LTDC);
    EXPECT_NE(consumer, nullptr);
    EXPECT_NE(system, nullptr);
    EXPECT_EQ(hardware->rcc.APB2ENR & RCC_APB2ENR_LTDCEN, 0U);
    ++hardware->graphics_attaches;
    return hardware->attach_result;
}
extern "C" nx_result_t
nx_stm32_sdram_graphics_check(const nx_stm32_sdram_graphics_t*,
                              nx_stm32_sdram_graphics_master_t master,
                              const void*, const void*) {
    EXPECT_EQ(master, NX_STM32_SDRAM_GRAPHICS_LTDC);
    ++hardware->graphics_checks;
    return hardware->check_result;
}
extern "C" nx_result_t
nx_stm32_sdram_graphics_detach(nx_stm32_sdram_graphics_t*,
                               nx_stm32_sdram_graphics_master_t master,
                               const void*) {
    EXPECT_EQ(master, NX_STM32_SDRAM_GRAPHICS_LTDC);
    EXPECT_EQ(hardware->rcc.APB2ENR & RCC_APB2ENR_LTDCEN, 0U);
    ++hardware->graphics_detaches;
    return hardware->detach_result;
}

extern "C" bool
nx_stm32_sdram_graphics_overlaps(const nx_stm32_sdram_graphics_t*,
                                 const void* storage, size_t bytes) {
    const auto first = reinterpret_cast<uintptr_t>(hardware->bank_metadata);
    const auto start = reinterpret_cast<uintptr_t>(storage);
    return start < first + sizeof(hardware->bank_metadata) &&
           first < start + bytes;
}

/**
 * \file            stm32_dma2d_test.cpp
 * \brief           Actual DMA2D HAL single-shot loans and finite withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_dma2d_model.h"
extern "C" {
#include "stm32f407_system.h"
#include "stm32f4_dma2d.h"
#include "stm32f4_graphics_memory.h"
}
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
struct Hardware {
    RCC_TypeDef rcc{};
    DMA2D_TypeDef registers{};
    nx_time_us_t now = 1;
    uint32_t enabled = 0, pending = 0, active = 0, priority = 7;
    bool task = true, refuse_abort = false, refuse_address = false;
    bool refuse_irq = false, refuse_priority = false;
    bool expire_on_config = false, refuse_color = false;
    bool refuse_background = false;
    bool transfer_error_on_abort = false;
    bool refuse_gate_enable = false, refuse_gate_disable = false;
    unsigned writes = 0, peripheral_reads = 0;
    unsigned graphics_attaches = 0, graphics_checks = 0, graphics_detaches = 0;
    alignas(8) uint8_t bank_metadata[64]{};
    nx_result_t attach_result = NX_SUCCESS, check_result = NX_SUCCESS;
    nx_result_t detach_result = NX_SUCCESS;
} * hardware;

/* External pixel memory behavior follows RM0386 10.3.4/6/7. This is not a
 * firmware pixel fallback or proof of physical image output. */
uint32_t pixel_read(const uint8_t* p, uint32_t format) {
    if (format == DMA2D_INPUT_RGB565) {
        uint16_t v;
        std::memcpy(&v, p, sizeof(v));
        const auto r = static_cast<uint32_t>((v >> 11U) & 31U);
        const auto g = static_cast<uint32_t>((v >> 5U) & 63U);
        const auto b = static_cast<uint32_t>(v & 31U);
        return UINT32_C(0xff000000) | (((r << 3U) | (r >> 2U)) << 16U) |
               (((g << 2U) | (g >> 4U)) << 8U) | (b << 3U) | (b >> 2U);
    }
    uint32_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
}
void pixel_write(uint8_t* p, uint32_t format, uint32_t argb) {
    if (format == DMA2D_OUTPUT_RGB565) {
        const auto packed = static_cast<uint16_t>(
            ((argb >> 8U) & UINT32_C(0xf800)) |
            ((argb >> 5U) & UINT32_C(0x07e0)) | ((argb >> 3U) & 31U));
        std::memcpy(p, &packed, sizeof(packed));
    } else {
        std::memcpy(p, &argb, sizeof(argb));
    }
}
uint32_t alpha(uint32_t argb, uint32_t control) {
    const uint32_t mode =
        (control & static_cast<uint32_t>(DMA2D_FGPFCCR_AM)) >> 16U;
    const uint32_t constant = control >> 24U;
    uint32_t value = argb >> 24U;
    if (mode == DMA2D_REPLACE_ALPHA)
        value = constant;
    else if (mode == DMA2D_COMBINE_ALPHA)
        value = value * constant / 255U;
    return (argb & UINT32_C(0x00ffffff)) | (value << 24U);
}
uint32_t blend(uint32_t foreground, uint32_t background) {
    const uint32_t fa = foreground >> 24U, ba = background >> 24U;
    const uint32_t cross = fa * ba / 255U, out = fa + ba - cross;
    if (out == 0U)
        return UINT32_C(
            0x00123456); /* RGB has no defined alpha-zero meaning. */
    uint32_t value = out << 24U;
    for (unsigned shift = 0; shift < 24U; shift += 8U) {
        const uint32_t f = (foreground >> shift) & 255U;
        const uint32_t b = (background >> shift) & 255U;
        value |= ((f * fa + b * ba - b * cross) / out) << shift;
    }
    return value;
}
void execute_pixels() {
    const auto& r = hardware->registers;
    const uint32_t mode = r.CR & DMA2D_CR_MODE;
    const uint32_t output = r.OPFCCR & DMA2D_OPFCCR_CM;
    const uint32_t foreground = r.FGPFCCR & DMA2D_FGPFCCR_CM;
    const uint32_t background = r.BGPFCCR & DMA2D_BGPFCCR_CM;
    const size_t width = r.NLR >> 16U, height = r.NLR & UINT32_C(0xffff);
    const size_t ob = output == DMA2D_OUTPUT_RGB565 ? 2U : 4U;
    const size_t fb = foreground == DMA2D_INPUT_RGB565 ? 2U : 4U;
    const size_t bb = background == DMA2D_INPUT_RGB565 ? 2U : 4U;
    auto* destination =
        reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(r.OMAR));
    const auto* fg =
        reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(r.FGMAR));
    const auto* bg =
        reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(r.BGMAR));
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            uint32_t value;
            if (mode == DMA2D_R2M) {
                if (ob == 2U) {
                    const auto word = static_cast<uint16_t>(r.OCOLR);
                    std::memcpy(destination + x * ob, &word, sizeof(word));
                    continue;
                }
                value = r.OCOLR;
            } else {
                value = pixel_read(fg + x * fb, foreground);
                if (mode != DMA2D_M2M)
                    value = alpha(value, r.FGPFCCR);
                if (mode == DMA2D_M2M_BLEND)
                    value =
                        blend(value, alpha(pixel_read(bg + x * bb, background),
                                           r.BGPFCCR));
            }
            pixel_write(destination + x * ob, output, value);
        }
        destination += (width + r.OOR) * ob;
        if (mode != DMA2D_R2M)
            fg += (width + r.FGOR) * fb;
        if (mode == DMA2D_M2M_BLEND)
            bg += (width + r.BGOR) * bb;
    }
}

class ImageOwner : public testing::Test {
  protected:
    Hardware model{};
    uint8_t* memory = nullptr;
    nx_stm32_system_t* system = nullptr;
    nx_dma_memory_region_t region{};
    /* Retained graphics metadata belongs to the whole fixture lifetime. */
    nx_stm32_sdram_graphics_t graphics_binding{};
    nx_stm32_image_config_t config{};
    nx_stm32_image_state_t state{};
    nx_image_request_t request{}, other{};
    nx_image_port_t port{&nx_stm32_image_ops, &state};

    void SetUp() override {
        hardware = &model;
        void* mapped = mmap(nullptr, 8192, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
        ASSERT_NE(mapped, MAP_FAILED);
        memory = static_cast<uint8_t*>(mapped);
        system = reinterpret_cast<nx_stm32_system_t*>(memory + 512);
        system->rcc = &model.rcc;
        system->flash = FLASH;
        system->started = true;
        system->core_hz = 16000000;
        system->time_epoch = 1;
        region = {reinterpret_cast<uintptr_t>(memory), 8192,
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        model.rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
        model.rcc.AHB1ENR = 0;
        config = {&model.registers,
                  &model.rcc,
                  &region,
                  1,
                  system,
                  {NX_IRQ_KERNEL_NONE, 93, 4, 0},
                  10,
                  0,
                  5,
                  nullptr};
        nx_request_initialize(&request.base);
        nx_request_initialize(&other.base);
    }
    void TearDown() override {
        if (memory != nullptr) {
            EXPECT_EQ(munmap(memory, 8192), 0);
        }
        hardware = nullptr;
    }
    void open() {
        ASSERT_EQ(nx_stm32_image_prepare(&state, &config), NX_SUCCESS);
    }
    void fill(nx_image_request_t* req = nullptr) {
        nx_image_operation_t op{};
        op.kind = NX_IMAGE_FILL;
        op.width = 4;
        op.height = 2;
        op.destination = {memory + 4096, 64, 16, NX_IMAGE_RGB565};
        op.fill_value = 0x6d5b;
        ASSERT_EQ(nx_image_prepare(req != nullptr ? req : &request, &op, 100),
                  NX_SUCCESS);
    }
    void copy() {
        nx_image_operation_t op{};
        op.kind = NX_IMAGE_COPY;
        op.width = 4;
        op.height = 2;
        op.foreground = {memory, 64, 12, NX_IMAGE_RGB565};
        op.destination = {memory + 4096, 64, 16, NX_IMAGE_RGB565};
        ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    }
    void irq() {
        model.active = 1;
        model.task = false;
        nx_stm32_image_irq(&state);
        model.task = true;
        model.active = 0;
    }
    void complete() {
        if ((model.registers.CR & DMA2D_CR_START) != 0U)
            execute_pixels();
        model.registers.CR &=
            ~static_cast<uint32_t>(DMA2D_CR_START | DMA2D_CR_ABORT);
        model.registers.ISR |= DMA2D_ISR_TCIF;
        irq();
    }
};

TEST_F(ImageOwner, PlatformAdmissionRunsBeforeSystemWithoutEffects) {
    system->started = false;
    system->time_epoch = 0;
    model.rcc.CR = 0;
    const auto before = state;
    EXPECT_EQ(nx_stm32_image_admit(&state, &config), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(&before, &state, sizeof(state)), 0);
    EXPECT_EQ(model.writes, 0U);
}

TEST_F(ImageOwner, PlatformAdmissionRefusesForeignGateOrVectorWithoutEffects) {
    model.rcc.AHB1ENR = RCC_AHB1ENR_DMA2DEN;
    EXPECT_EQ(nx_stm32_image_admit(&state, &config), NX_ERROR_BUSY);
    model.rcc.AHB1ENR = 0;
    model.pending = 1;
    EXPECT_EQ(nx_stm32_image_admit(&state, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(model.writes, 0U);
}

TEST_F(ImageOwner, ForeignVectorIsRejectedBeforeAnyHardwareOrOwnerEffect) {
    model.pending = 1;
    EXPECT_EQ(nx_stm32_image_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(model.writes, 0U);
    EXPECT_FALSE(state.owned);
}

TEST_F(ImageOwner, ColdOwnerAcquiresAndReturnsOnlyItsControllerGate) {
    model.rcc.AHB1ENR = RCC_AHB1ENR_GPIOAEN;
    open();
    ASSERT_TRUE(state.gate_owned);
    EXPECT_EQ(model.rcc.AHB1ENR,
              static_cast<uint32_t>(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2DEN));
    ASSERT_EQ(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(state.gate_owned);
    EXPECT_EQ(model.rcc.AHB1ENR, static_cast<uint32_t>(RCC_AHB1ENR_GPIOAEN));
}

TEST_F(ImageOwner, APreownedControllerGateIsRefusedBeforeColdEffects) {
    model.rcc.AHB1ENR |= RCC_AHB1ENR_DMA2DEN;
    EXPECT_EQ(nx_stm32_image_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(model.writes, 0U);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(ImageOwner, ARefusedGateEnableKeepsARetryableColdReceipt) {
    model.rcc.AHB1ENR = 0;
    model.refuse_gate_enable = true;
    EXPECT_EQ(nx_stm32_image_prepare(&state, &config), NX_ERROR_IO);
    EXPECT_TRUE(state.owned);
    EXPECT_TRUE(state.gate_owned);
    EXPECT_EQ(state.config, &config);
    EXPECT_EQ(model.registers.CR, 0U);
    model.refuse_gate_enable = false;
    EXPECT_EQ(nx_stm32_image_close(&state), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_FALSE(state.gate_owned);
}

TEST_F(ImageOwner, ARefusedGateDisableRetainsTheSameColdConfig) {
    model.rcc.AHB1ENR = 0;
    open();
    model.refuse_gate_disable = true;
    EXPECT_NE(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(state.config, &config);
    EXPECT_TRUE(state.gate_owned);
    model.refuse_gate_disable = false;
    EXPECT_EQ(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
}

TEST_F(ImageOwner, QueryAdmitsNativeFillCopyAndExplicitPfcOrBlendFormats) {
    open();
    nx_image_constraints_t limits{};
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_FILL, NX_IMAGE_RGB565,
                                  NX_IMAGE_RGB565, NX_IMAGE_RGB565, &limits),
              NX_SUCCESS);
    EXPECT_EQ(limits.maximum_width, 16383U);
    EXPECT_EQ(limits.maximum_height, 65535U);
    EXPECT_EQ(limits.maximum_offset_pixels, 16383U);
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_CONVERT, NX_IMAGE_RGB565,
                                  NX_IMAGE_RGB565, NX_IMAGE_ARGB8888, &limits),
              NX_SUCCESS);
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_COPY, NX_IMAGE_RGB565,
                                  NX_IMAGE_RGB565, NX_IMAGE_ARGB8888, &limits),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_BLEND, NX_IMAGE_RGB565,
                                  NX_IMAGE_ARGB8888, NX_IMAGE_ARGB8888,
                                  &limits),
              NX_SUCCESS);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_START, 0U);
}

TEST_F(ImageOwner, HALFillPreservesTheExactEncoded565Value) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.OCOLR, 0x6d5bU);
    EXPECT_EQ(model.registers.OOR, 4U);
    EXPECT_EQ(model.registers.NLR, (4U << 16) | 2U);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_MODE, DMA2D_R2M);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_START, DMA2D_CR_START);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    complete();
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 16U);
    EXPECT_EQ(model.registers.OMAR, 0U);
}

TEST_F(ImageOwner, HALCopyUsesSeparatePixelOffsetsAndFiniteByteCount) {
    open();
    copy();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.FGOR, 2U);
    EXPECT_EQ(model.registers.OOR, 4U);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_MODE, DMA2D_M2M);
    EXPECT_EQ(model.registers.FGMAR,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(memory)));
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(model.registers.FGMAR, 0U);
    EXPECT_EQ(model.registers.BGMAR, 0U);
    EXPECT_EQ(request.base.transferred, 16U);
}

TEST_F(ImageOwner, ACompleteHintWithStartStillLiveCannotReleaseAnySpan) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    model.registers.ISR |= DMA2D_ISR_TCIF;
    irq();
    model.refuse_abort = true;
    EXPECT_EQ(nx_image_port_service(&port), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(model.registers.OMAR, 0U);
}

TEST_F(ImageOwner, CancelIssuesAbortAndQuarantinesUntilTheEngineActuallyStops) {
    open();
    copy();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    model.refuse_abort = true;
    ASSERT_EQ(nx_image_port_cancel(&port, &request), NX_SUCCESS);
    EXPECT_EQ(nx_image_port_service(&port), NX_ERROR_BUSY);
    model.now = 20;
    EXPECT_EQ(nx_image_port_service(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(model.registers.FGMAR, 0U);
    model.refuse_abort = false;
    EXPECT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(ImageOwner, TerminalCauseSurvivesLateErrorAndCancelHints) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    model.refuse_abort = true;
    model.now = 100;
    EXPECT_EQ(nx_image_port_service(&port), NX_ERROR_BUSY);
    model.registers.ISR |= DMA2D_ISR_TEIF;
    irq();
    EXPECT_EQ(nx_image_port_cancel(&port, &request), NX_SUCCESS);
    model.refuse_abort = false;
    EXPECT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
}

TEST_F(ImageOwner, ErrorIRQIsNotTaskSettlementAndDoesNotClaimAValidPrefix) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    model.registers.CR &= ~static_cast<uint32_t>(DMA2D_CR_START);
    model.registers.ISR |= DMA2D_ISR_TEIF;
    irq();
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(ImageOwner, RefusedAddressWithdrawalRetainsTheLoanAndRetries) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    model.refuse_address = true;
    EXPECT_NE(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    model.refuse_address = false;
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(ImageOwner, LiveLoansAndAllPrivateResourcesExcludeQueryAndPayloadAlias) {
    open();
    fill();
    request.operation.destination.data = &state;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_INVALID);
    request.operation.destination.data = &config;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_INVALID);
    request.operation.destination.data = &region;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_INVALID);
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    auto* in_frame = reinterpret_cast<nx_image_constraints_t*>(memory + 4096);
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_FILL, NX_IMAGE_RGB565,
                                  NX_IMAGE_RGB565, NX_IMAGE_RGB565, in_frame),
              NX_ERROR_INVALID);
    auto* in_request = reinterpret_cast<nx_image_constraints_t*>(&request);
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_FILL, NX_IMAGE_RGB565,
                                  NX_IMAGE_RGB565, NX_IMAGE_RGB565, in_request),
              NX_ERROR_INVALID);
}

TEST_F(ImageOwner, RejectionLeavesRequestAndRegistersUnchanged) {
    open();
    fill();
    model.now = 100;
    const auto before = request;
    const auto regs = model.registers;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(std::memcmp(&before, &request, sizeof(request)), 0);
    EXPECT_EQ(std::memcmp(&regs, &model.registers, sizeof(regs)), 0);
}

TEST_F(ImageOwner, CpuOnlySdramAddressDoesNotGrantAnImageBusMasterLoan) {
    open();
    fill();
    request.operation.destination.data =
        reinterpret_cast<void*>(UINT32_C(0xc0000000));
    const auto before = request;
    const auto registers = model.registers;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_image_port_submit(&port, &request));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
    EXPECT_EQ(0, std::memcmp(&registers, &model.registers, sizeof(registers)));
    EXPECT_EQ(nullptr, state.active);
}

TEST_F(ImageOwner, RefusedIRQJoinAndPriorityRestoreRetainColdOwnership) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    model.refuse_irq = true;
    EXPECT_NE(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    model.refuse_irq = false;
    model.refuse_priority = true;
    EXPECT_NE(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    model.refuse_priority = false;
    ASSERT_EQ(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(model.priority, 7U);
}

TEST_F(ImageOwner, LostSystemClockCannotBeTurnedIntoAFalseDrainProof) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    model.rcc.AHB1ENR = 0;
    model.pending = 1;
    const auto reads = model.peripheral_reads;
    irq();
    EXPECT_EQ(model.peripheral_reads, reads);
    EXPECT_EQ(model.enabled, 0U);
    EXPECT_TRUE(state.clock_fault);
    EXPECT_EQ(nx_image_port_service(&port), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    model.rcc.AHB1ENR = RCC_AHB1ENR_DMA2DEN;
    EXPECT_NE(nx_image_port_stop(&port), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(ImageOwner,
       ClosedStaticOwnerCanBePreparedAndDoesNotTouchReturnedConfig) {
    open();
    ASSERT_EQ(nx_image_port_stop(&port), NX_SUCCESS);
    config.registers = nullptr;
    EXPECT_EQ(nx_image_port_stop(&port), NX_SUCCESS);
    nx_stm32_image_irq(&state);
    config.registers = &model.registers;
    ASSERT_EQ(nx_stm32_image_prepare(&state, &config), NX_SUCCESS);
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    EXPECT_EQ(nx_image_port_service(&port), NX_SUCCESS);
}

TEST_F(ImageOwner, ExpirationDuringHALConfigurationDoesNotLaunchPixels) {
    open();
    fill();
    model.expire_on_config = true;
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_START, 0U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(ImageOwner, RefusedHALPixelFormatCannotStartWithAnUnsafeOutputWidth) {
    open();
    fill();
    model.refuse_color = true;
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_START, 0U);
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(ImageOwner, AnInFlightBusFaultDuringAbortRetainsLoanUntilWithdrawal) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_cancel(&port, &request), NX_SUCCESS);
    model.transfer_error_on_abort = true;
    model.refuse_address = true;
    EXPECT_NE(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_NE(model.registers.OMAR, 0U);
    model.refuse_address = false;
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(ImageOwner, AnUnservicedBusErrorCannotBeClearedIntoTimeout) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    /* AHB error clears START independently of the IRQ enable/dispatch. */
    model.registers.CR &= ~static_cast<uint32_t>(DMA2D_CR_START);
    model.registers.ISR |= DMA2D_ISR_TEIF;
    model.pending = 1;
    model.now = 100;
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(ImageOwner, CancellationAfterAProvenCompleteHintPreservesSuccess) {
    open();
    fill();
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    ASSERT_EQ(nx_image_port_cancel(&port, &request), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 16U);
}

TEST_F(ImageOwner, Convert565To8888ReplicatesMsbsAndUsesSourcePixelStride) {
    open();
    const uint16_t source[] = {0xf800, 0x07e0, 0x001f, 0x6d5b};
    std::memcpy(memory, source, sizeof(source));
    std::memcpy(memory + 12, source, sizeof(source));
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_CONVERT;
    op.foreground = {memory, 64, 12, NX_IMAGE_RGB565};
    op.destination = {memory + 4096, 128, 24, NX_IMAGE_ARGB8888};
    op.width = 4;
    op.height = 2;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_MODE, DMA2D_M2M_PFC);
    EXPECT_EQ(model.registers.FGOR, 2U);
    EXPECT_EQ(model.registers.OOR, 2U);
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    const uint32_t expected[] = {0xffff0000, 0xff00ff00, 0xff0000ff,
                                 0xff6baade};
    EXPECT_EQ(std::memcmp(memory + 4096, expected, sizeof(expected)), 0);
    EXPECT_EQ(std::memcmp(memory + 4096 + 24, expected, sizeof(expected)), 0);
    EXPECT_EQ(request.base.transferred, 32U);
}

TEST_F(ImageOwner, Convert8888To565TruncatesChannelsAndIgnoresSourceAlpha) {
    open();
    const uint32_t source[] = {0x12345678, 0xff0000ff, 0xff00ff00, 0xffff0000};
    std::memcpy(memory, source, sizeof(source));
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_CONVERT;
    op.foreground = {memory, 64, 20, NX_IMAGE_ARGB8888};
    op.destination = {memory + 4096, 64, 12, NX_IMAGE_RGB565};
    op.width = 4;
    op.height = 1;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.FGOR, 1U);
    EXPECT_EQ(model.registers.OOR, 2U);
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    const uint16_t expected[] = {0x32af, 0x001f, 0x07e0, 0xf800};
    EXPECT_EQ(std::memcmp(memory + 4096, expected, sizeof(expected)), 0);
    EXPECT_EQ(request.base.transferred, 8U);
}

TEST_F(ImageOwner, BlendHasIndependentBackgroundFormatStrideAndAlpha) {
    open();
    const uint16_t fg[] = {0xf800, 0x07e0};
    const uint32_t bg[] = {0xff0000ff, 0xff0000ff};
    std::memcpy(memory, fg, sizeof(fg));
    std::memcpy(memory + 2048, bg, sizeof(bg));
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_BLEND;
    op.foreground = {memory, 64, 8, NX_IMAGE_RGB565};
    op.background = {memory + 2048, 64, 16, NX_IMAGE_ARGB8888};
    op.destination = {memory + 4096, 64, 12, NX_IMAGE_ARGB8888};
    op.width = 2;
    op.height = 1;
    op.foreground_alpha = 128;
    op.background_alpha = 255;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ(model.registers.CR & DMA2D_CR_MODE, DMA2D_M2M_BLEND);
    EXPECT_EQ(model.registers.FGOR, 2U);
    EXPECT_EQ(model.registers.BGOR, 2U);
    EXPECT_EQ(model.registers.OOR, 1U);
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    const uint32_t expected[] = {0xff80007f, 0xff00807f};
    EXPECT_EQ(std::memcmp(memory + 4096, expected, sizeof(expected)), 0);
}

TEST_F(ImageOwner, BlendCombinesPerPixelAlphaWithoutPremultiplyingTheInput) {
    open();
    const uint32_t fg = 0x80ff0000, bg = 0xff0000ff;
    std::memcpy(memory, &fg, sizeof(fg));
    std::memcpy(memory + 2048, &bg, sizeof(bg));
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_BLEND;
    op.foreground = {memory, 16, 4, NX_IMAGE_ARGB8888};
    op.background = {memory + 2048, 16, 4, NX_IMAGE_ARGB8888};
    op.destination = {memory + 4096, 16, 4, NX_IMAGE_ARGB8888};
    op.width = 1;
    op.height = 1;
    op.foreground_alpha = 128;
    op.background_alpha = 255;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    uint32_t actual;
    std::memcpy(&actual, memory + 4096, sizeof(actual));
    EXPECT_EQ(actual, 0xff4000bfU);
}

TEST_F(ImageOwner, BlendTo565RequiresAProvablyNonzeroOutputAlpha) {
    open();
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_BLEND;
    op.foreground = {memory, 16, 4, NX_IMAGE_ARGB8888};
    op.background = {memory + 2048, 16, 4, NX_IMAGE_ARGB8888};
    op.destination = {memory + 4096, 16, 4, NX_IMAGE_RGB565};
    op.width = 1;
    op.height = 1;
    op.foreground_alpha = 255;
    op.background_alpha = 255;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    const auto before = request;
    const auto writes = model.writes;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(std::memcmp(&before, &request, sizeof(request)), 0);
    EXPECT_EQ(model.writes, writes);
}

TEST_F(ImageOwner, FullyTransparentArgbBlendDoesNotInventAnRgbGuarantee) {
    open();
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_BLEND;
    op.foreground = {memory, 16, 4, NX_IMAGE_ARGB8888};
    op.background = {memory + 2048, 16, 4, NX_IMAGE_ARGB8888};
    op.destination = {memory + 4096, 16, 4, NX_IMAGE_ARGB8888};
    op.width = 1;
    op.height = 1;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    uint32_t actual;
    std::memcpy(&actual, memory + 4096, sizeof(actual));
    EXPECT_EQ(actual >> 24U, 0U);
}

TEST_F(ImageOwner, ReplaceAlphaProvesPositive565OutputWithoutReadingPixels) {
    open();
    const uint32_t foreground = 0x00ff0000, background = 0x000000ff;
    std::memcpy(memory, &foreground, sizeof(foreground));
    std::memcpy(memory + 2048, &background, sizeof(background));
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_BLEND;
    op.foreground = {memory, 16, 4, NX_IMAGE_ARGB8888};
    op.background = {memory + 2048, 16, 4, NX_IMAGE_ARGB8888};
    op.destination = {memory + 4096, 16, 4, NX_IMAGE_RGB565};
    op.width = 1;
    op.height = 1;
    op.foreground_alpha_mode = NX_IMAGE_ALPHA_REPLACE;
    op.foreground_alpha = 1;
    op.background_alpha = 255;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    EXPECT_EQ((model.registers.FGPFCCR & DMA2D_FGPFCCR_AM) >> 16U,
              DMA2D_REPLACE_ALPHA);
    complete();
    ASSERT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    uint16_t actual;
    std::memcpy(&actual, memory + 4096, sizeof(actual));
    EXPECT_EQ(actual, 0xf800U);
}

TEST_F(ImageOwner, BackgroundAddressJoinRetainsEveryLoanUntilActualWithdrawal) {
    open();
    nx_image_operation_t op{};
    op.kind = NX_IMAGE_BLEND;
    op.foreground = {memory, 16, 4, NX_IMAGE_RGB565};
    op.background = {memory + 2048, 16, 4, NX_IMAGE_RGB565};
    op.destination = {memory + 4096, 16, 4, NX_IMAGE_RGB565};
    op.width = 1;
    op.height = 1;
    op.foreground_alpha = 128;
    op.background_alpha = 255;
    ASSERT_EQ(nx_image_prepare(&request, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_port_submit(&port, &request), NX_SUCCESS);
    complete();
    model.refuse_background = true;
    EXPECT_NE(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_NE(model.registers.BGMAR, 0U);
    model.refuse_background = false;
    EXPECT_EQ(nx_image_port_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
}

TEST_F(ImageOwner, ProductionAdmissionExcludesTheWholeConcreteSystemOwner) {
    open();
    fill();
    request.operation.destination.data = &system->rcc;
    request.operation.destination.capacity = 16;
    request.operation.width = 1;
    request.operation.height = 1;
    request.operation.destination.stride_bytes = 4;
    const auto before = *system;
    const auto writes = model.writes;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(std::memcmp(&before, system, sizeof(before)), 0);
    EXPECT_EQ(model.writes, writes);
}

TEST_F(ImageOwner, GraphicsAttachmentRefusalPrecedesAnyControllerEffect) {
    config.graphics_memory = &graphics_binding;
    model.attach_result = NX_ERROR_STATE;
    const auto writes = model.writes;
    EXPECT_EQ(nx_stm32_image_prepare(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(model.graphics_attaches, 1U);
    EXPECT_EQ(model.writes, writes);
    EXPECT_EQ(model.rcc.AHB1ENR & RCC_AHB1ENR_DMA2DEN, 0U);
    EXPECT_EQ(nx_stm32_image_close(&state), NX_SUCCESS);
    EXPECT_EQ(model.graphics_detaches, 0U);
}
TEST_F(ImageOwner, GraphicsHoldSurvivesFailedIrqJoinUntilActualColdClose) {
    config.graphics_memory = &graphics_binding;
    open();
    ASSERT_EQ(model.graphics_attaches, 1U);
    model.refuse_irq = true;
    EXPECT_EQ(nx_stm32_image_close(&state), NX_ERROR_BUSY);
    EXPECT_EQ(model.graphics_detaches, 0U);
    EXPECT_TRUE(state.memory_attached);
    model.refuse_irq = false;
    EXPECT_EQ(nx_stm32_image_close(&state), NX_SUCCESS);
    EXPECT_EQ(model.graphics_detaches, 1U);
    EXPECT_FALSE(state.memory_attached);
}
TEST_F(ImageOwner,
       GraphicsDetachFailureRetainsOriginalOwnerAndCanRetryAfterGateOff) {
    config.graphics_memory = &graphics_binding;
    open();
    model.detach_result = NX_ERROR_STATE;
    EXPECT_EQ(nx_stm32_image_close(&state), NX_ERROR_STATE);
    EXPECT_TRUE(state.owned);
    EXPECT_TRUE(state.memory_attached);
    EXPECT_EQ(model.rcc.AHB1ENR & RCC_AHB1ENR_DMA2DEN, 0U);
    model.detach_result = NX_SUCCESS;
    EXPECT_EQ(nx_stm32_image_close(&state), NX_SUCCESS);
    EXPECT_EQ(model.graphics_detaches, 2U);
    EXPECT_FALSE(state.owned);
}
TEST_F(ImageOwner, LostGraphicsBankEvidenceRejectsBeforeRequestBorrow) {
    config.graphics_memory = &graphics_binding;
    open();
    fill();
    model.check_result = NX_ERROR_STATE;
    const auto writes = model.writes;
    EXPECT_EQ(nx_image_port_submit(&port, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(model.writes, writes);
    EXPECT_EQ(model.graphics_checks, 1U);
    EXPECT_EQ(nx_stm32_image_close(&state), NX_SUCCESS);
}

TEST_F(ImageOwner, QueryCannotOverwriteTheRetainedBankOwnerMetadata) {
    config.graphics_memory = &graphics_binding;
    open();
    auto* output =
        reinterpret_cast<nx_image_constraints_t*>(model.bank_metadata);
    const auto before = model;
    EXPECT_EQ(nx_image_port_query(&port, NX_IMAGE_FILL, NX_IMAGE_RGB565,
                                  NX_IMAGE_RGB565, NX_IMAGE_RGB565, output),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(model.bank_metadata, before.bank_metadata,
                          sizeof(model.bank_metadata)),
              0);
}

} /* namespace */

extern "C" uint32_t nx_img_model_read(volatile uint32_t* reg) {
    const auto address = reinterpret_cast<uintptr_t>(reg);
    const auto begin = reinterpret_cast<uintptr_t>(&hardware->registers);
    if (address >= begin && address < begin + sizeof(hardware->registers))
        ++hardware->peripheral_reads;
    return *reg;
}
extern "C" void nx_img_model_write(volatile uint32_t* reg, uint32_t value) {
    ++hardware->writes;
    if (reg == &hardware->rcc.AHB1ENR &&
        (((value & RCC_AHB1ENR_DMA2DEN) != 0U &&
          hardware->refuse_gate_enable) ||
         ((value & RCC_AHB1ENR_DMA2DEN) == 0U &&
          hardware->refuse_gate_disable)))
        return;
    auto& r = hardware->registers;
    if (reg == &r.BGMAR && value == 0U && hardware->refuse_background)
        return;
    if (reg == &r.OPFCCR) {
        if (hardware->expire_on_config)
            hardware->now = 100;
        if (hardware->refuse_color)
            return;
    }
    if (reg == &r.IFCR) {
        r.ISR &= ~value;
        *reg = value;
    } else if ((reg == &r.OMAR || reg == &r.FGMAR || reg == &r.BGMAR ||
                reg == &r.FGCMAR || reg == &r.BGCMAR) &&
               value == 0U && hardware->refuse_address) {
        return;
    } else {
        *reg = value;
        if (reg == &r.CR && (value & DMA2D_CR_ABORT) != 0U &&
            !hardware->refuse_abort) {
            /* An outstanding AHB beat may report its error as abort retires. */
            if (hardware->transfer_error_on_abort) {
                r.ISR |= DMA2D_ISR_TEIF;
                hardware->pending = 1;
            }
            r.CR &= ~static_cast<uint32_t>(DMA2D_CR_START | DMA2D_CR_ABORT |
                                           DMA2D_CR_SUSP);
        }
    }
}
extern "C" nx_time_us_t nx_img_model_now(void) {
    return hardware->now;
}
extern "C" bool nx_img_model_task(void) {
    return hardware->task;
}
extern "C" uint32_t nx_img_model_irq_enabled(void) {
    return hardware->enabled;
}
extern "C" uint32_t nx_img_model_irq_pending(void) {
    return hardware->pending;
}
extern "C" uint32_t nx_img_model_irq_active(void) {
    return hardware->active;
}
extern "C" void nx_img_model_irq_enable(void) {
    hardware->enabled = 1;
}
extern "C" void nx_img_model_irq_disable(void) {
    if (!hardware->refuse_irq)
        hardware->enabled = 0;
}
extern "C" void nx_img_model_irq_clear(void) {
    hardware->pending = 0;
}
extern "C" uint32_t nx_img_model_irq_priority(void) {
    return hardware->priority;
}
extern "C" void nx_img_model_irq_set_priority(uint32_t priority) {
    if (!hardware->refuse_priority)
        hardware->priority = priority;
}
extern "C" uint32_t HAL_GetTick(void) {
    return static_cast<uint32_t>(hardware->now / 1000U);
}

/* The SDRAM owner is an external lifetime dependency in this engine model.
 * Its original-lease rules are tested by the real SDRAM provider program. */
extern "C" nx_result_t
nx_stm32_sdram_graphics_attach(nx_stm32_sdram_graphics_t*,
                               nx_stm32_sdram_graphics_master_t master,
                               const void* consumer, const void* system) {
    EXPECT_EQ(master, NX_STM32_SDRAM_GRAPHICS_DMA2D);
    EXPECT_NE(consumer, nullptr);
    EXPECT_NE(system, nullptr);
    EXPECT_EQ(hardware->rcc.AHB1ENR & RCC_AHB1ENR_DMA2DEN, 0U);
    ++hardware->graphics_attaches;
    return hardware->attach_result;
}
extern "C" nx_result_t
nx_stm32_sdram_graphics_check(const nx_stm32_sdram_graphics_t*,
                              nx_stm32_sdram_graphics_master_t master,
                              const void*, const void*) {
    EXPECT_EQ(master, NX_STM32_SDRAM_GRAPHICS_DMA2D);
    ++hardware->graphics_checks;
    return hardware->check_result;
}
extern "C" nx_result_t
nx_stm32_sdram_graphics_detach(nx_stm32_sdram_graphics_t*,
                               nx_stm32_sdram_graphics_master_t master,
                               const void*) {
    EXPECT_EQ(master, NX_STM32_SDRAM_GRAPHICS_DMA2D);
    EXPECT_EQ(hardware->rcc.AHB1ENR & RCC_AHB1ENR_DMA2DEN, 0U);
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

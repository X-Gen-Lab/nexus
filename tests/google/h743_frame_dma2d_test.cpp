/**
 * \file            h743_frame_dma2d_test.cpp
 * \brief           Real DMA2D register ordering, rejection and retained loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_frame_dma2d_model.h"
extern "C" {
#include "nexus/components/frame_dma2d.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

DMA2D_TypeDef g_frame_dma2d;
RCC_TypeDef g_frame_rcc;
SCB_Type g_frame_core = {};
DBGMCU_TypeDef g_frame_debug;
namespace {
using testing::_;
using testing::NiceMock;
struct Hardware {
    MOCK_METHOD(uint32_t, read, (volatile uint32_t*));
    MOCK_METHOD(void, write, (volatile uint32_t*, uint32_t));
    bool irq_quiet = true;
    bool refuse_abort = false;
    bool refuse_detach = false;
    void write_register(volatile uint32_t* p, uint32_t value) {
        if (p == &g_frame_dma2d.IFCR) {
            g_frame_dma2d.ISR &= ~value;
        } else if (p == &g_frame_rcc.AHB3RSTR &&
                   (value & RCC_AHB3RSTR_DMA2DRST) != 0U) {
            std::memset(&g_frame_dma2d, 0, sizeof(g_frame_dma2d));
            *p = value;
        } else if (p == &g_frame_dma2d.CR && (value & DMA2D_CR_ABORT) != 0U &&
                   refuse_abort) {
            *p = value | DMA2D_CR_START;
        } else if ((p == &g_frame_dma2d.FGMAR || p == &g_frame_dma2d.OMAR) &&
                   value == 0U && refuse_detach) {
            return;
        } else {
            *p = value;
        }
    }
    Hardware() {
        ON_CALL(*this, read(_)).WillByDefault([](volatile uint32_t* p) {
            return *p;
        });
        ON_CALL(*this, write(_, _))
            .WillByDefault([this](volatile uint32_t* p, uint32_t v) {
                write_register(p, v);
            });
    }
};
Hardware* hardware;

class H743Frame : public testing::Test {
  protected:
    NiceMock<Hardware> hw;
    nx_h743_frame_dma2d_storage_t storage = {};
    const nx_frame_blitter_t face = {&nx_h743_frame_dma2d_ops, &storage};
    nx_h743_frame_dma2d_config_t config = {NX_H743_FRAME_DMA2D_CONTROLLER, 2U};
    nx_stream_slot_t slot = {};
    nx_stream_t stream = {};
    nx_frame_stream_t framed = {};
    nx_frame_dma_loan_t loan = {};
    nx_frame_blit_request_t request = {};
    uint8_t* source = reinterpret_cast<uint8_t*>(0x24000000U);
    uint8_t* destination = reinterpret_cast<uint8_t*>(0x24001000U);
    void* mapping = MAP_FAILED;
    void SetUp() override {
        hardware = &hw;
        std::memset(&g_frame_dma2d, 0, sizeof(g_frame_dma2d));
        std::memset(&g_frame_rcc, 0, sizeof(g_frame_rcc));
        g_frame_core.CCR = 0U;
        std::memset(&g_frame_debug, 0, sizeof(g_frame_debug));
        g_frame_debug.IDCODE = 0x20030450U;
        mapping =
            mmap(source, 8192U, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        ASSERT_EQ(mapping, source);
        std::memset(mapping, 0, 8192U);
        slot.data = source;
        slot.capacity = 64U;
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        const nx_frame_plane_t plane = {0U, 8U, 12U, 3U};
        nx_frame_layout_t layout = {};
        ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
                  NX_SUCCESS);
        nx_stream_fill_t fill = {};
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        for (unsigned i = 0; i < 32U; ++i) {
            source[i] = static_cast<uint8_t>(i + 1U);
        }
        ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
        nx_request_initialize(&request.base);
        ASSERT_EQ(nx_request_prepare(&request.base, 100U), NX_SUCCESS);
        request.source = &loan;
        request.x = 2U;
        request.y = 1U;
        request.width_bytes = 4U;
        request.rows = 2U;
        request.destination = destination;
        request.destination_stride = 8U;
        request.destination_capacity = 12U;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 8192U), 0);
        }
        hardware = nullptr;
    }
    void initialize() {
        ASSERT_EQ(nx_h743_frame_dma2d_initialize(&storage, &face, &config),
                  NX_SUCCESS);
    }
    void submit() {
        ASSERT_EQ(face.ops->submit(face.context, &request, 1U), NX_SUCCESS);
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    }
    void complete() {
        /* The external engine advances only when the test injects completion.
         * This copies from the programmed hardware descriptors, not requests.
         */
        const auto width = (g_frame_dma2d.NLR >> 16U) * 2U;
        const auto rows = g_frame_dma2d.NLR & 0xFFFFU;
        auto* src = reinterpret_cast<const uint8_t*>(
            static_cast<uintptr_t>(g_frame_dma2d.FGMAR));
        auto* dst = reinterpret_cast<uint8_t*>(
            static_cast<uintptr_t>(g_frame_dma2d.OMAR));
        for (unsigned row = 0U; row < rows; ++row) {
            std::memcpy(dst, src, width);
            src += width + g_frame_dma2d.FGOR * 2U;
            dst += width + g_frame_dma2d.OOR * 2U;
        }
        g_frame_dma2d.CR &= ~static_cast<uint32_t>(DMA2D_CR_START);
        g_frame_dma2d.ISR |= DMA2D_ISR_TCIF;
    }
    void expect_settled(nx_result_t expected, size_t transferred) {
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        nx_result_t result = NX_ERROR_IO;
        size_t bytes = SIZE_MAX;
        ASSERT_EQ(nx_request_result(&request.base, &result, &bytes),
                  NX_SUCCESS);
        EXPECT_EQ(result, expected);
        EXPECT_EQ(bytes, transferred);
        EXPECT_EQ(loan.owner, nullptr);
        EXPECT_EQ(slot.state, NX_STREAM_SLOT_FREE);
        EXPECT_EQ(g_frame_dma2d.FGMAR, 0U);
        EXPECT_EQ(g_frame_dma2d.OMAR, 0U);
        EXPECT_EQ(g_frame_dma2d.CR & DMA2D_CR_START, 0U);
    }
};

TEST_F(H743Frame, ConfiguresRealPixelOffsetsAndCopiesOnlySelectedRows) {
    initialize();
    EXPECT_CALL(hw, write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_START)).Times(1);
    submit();
    EXPECT_EQ(g_frame_dma2d.FGMAR, 0x2400000EU);
    EXPECT_EQ(g_frame_dma2d.OMAR, 0x24001000U);
    EXPECT_EQ(g_frame_dma2d.FGOR, 4U);
    EXPECT_EQ(g_frame_dma2d.OOR, 2U);
    EXPECT_EQ(g_frame_dma2d.NLR, (2U << 16U) | 2U);
    EXPECT_EQ(g_frame_dma2d.FGPFCCR, 2U);
    EXPECT_EQ(g_frame_dma2d.OPFCCR, 2U);
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    complete();
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    expect_settled(NX_SUCCESS, 8U);
    EXPECT_EQ(std::memcmp(destination, source + 14U, 4U), 0);
    EXPECT_EQ(std::memcmp(destination + 8U, source + 26U, 4U), 0);
    EXPECT_EQ(destination[4], 0U);
    EXPECT_EQ(destination[7], 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    EXPECT_EQ(g_frame_rcc.AHB3ENR & RCC_AHB3ENR_DMA2DEN, 0U);
}

TEST_F(H743Frame, CounterOrTcFlagAloneNeverReleasesAnActiveEngine) {
    initialize();
    submit();
    g_frame_dma2d.ISR = DMA2D_ISR_TCIF;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    complete();
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    expect_settled(NX_SUCCESS, 8U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, FailedAbortQuarantinesAndRetainsEveryBorrow) {
    initialize();
    submit();
    hw.refuse_abort = true;
    EXPECT_CALL(hw, write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_ABORT)).Times(1);
    EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(face.ops->stop(face.context), NX_ERROR_BUSY);
    nx_stream_fill_t fill = {};
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_OVERFLOW);
    hw.refuse_abort = false;
    g_frame_dma2d.CR = 0U;
    EXPECT_EQ(face.ops->service(face.context, 4U), NX_SUCCESS);
    expect_settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, TimeoutStartsAbortAndNeverSubstitutesForFinalAccess) {
    initialize();
    submit();
    hw.refuse_abort = true;
    EXPECT_EQ(face.ops->service(face.context, 100U), NX_ERROR_BUSY);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    hw.refuse_abort = false;
    g_frame_dma2d.CR = 0U;
    EXPECT_EQ(face.ops->service(face.context, 101U), NX_SUCCESS);
    expect_settled(NX_ERROR_TIMEOUT, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, TransferErrorDrainsThenSettlesAnUnquantifiedPartial) {
    initialize();
    submit();
    g_frame_dma2d.ISR = DMA2D_ISR_TEIF;
    g_frame_dma2d.CR = 0U;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    expect_settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, FailedAddressDetachRetainsSourceEvenAfterStartClears) {
    initialize();
    submit();
    complete();
    hw.refuse_detach = true;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    hw.refuse_detach = false;
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    expect_settled(NX_SUCCESS, 8U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsUnreachableOverlapAndUnalignedRectanglesBeforeStart) {
    initialize();
    EXPECT_CALL(hw, write(_, _)).Times(0);
    request.destination = reinterpret_cast<uint8_t*>(0x20000000U);
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U),
              NX_ERROR_PERMISSION);
    request.destination = source + 16U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.destination = destination;
    request.x = 1U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.x = 2U;
    request.destination_stride = 7U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.destination_stride = 8U;
    request.destination_capacity = 11U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    testing::Mock::VerifyAndClearExpectations(&hw);
    nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsWritesIntoUnselectedBytesOfSameImmutableFrame) {
    initialize();
    request.destination = source;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    if (nx_request_state(&request.base) == NX_REQUEST_ACTIVE) {
        EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
        EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    } else {
        nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
        EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    }
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsDestinationInsideSourceSlotCapacityTail) {
    initialize();
    request.destination = source + 48U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    if (nx_request_state(&request.base) == NX_REQUEST_ACTIVE) {
        EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
        EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    } else {
        nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
        EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    }
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsDestinationArithmeticOverflowBeforeAnyHardwareEffect) {
    initialize();
    EXPECT_CALL(hw, write(_, _)).Times(0);
    request.destination_stride = SIZE_MAX - 1U;
    request.rows = 3U;
    request.y = 0U;
    request.destination_capacity = SIZE_MAX;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsExpiredAdmissionAndForeignRequestCancellation) {
    initialize();
    EXPECT_EQ(face.ops->submit(face.context, &request, 100U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    submit();
    nx_frame_blit_request_t other = {};
    nx_request_initialize(&other.base);
    EXPECT_EQ(face.ops->cancel(face.context, &other), NX_ERROR_STATE);
    complete();
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    expect_settled(NX_SUCCESS, 8U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, LostCacheGuardCannotSettleAsSuccessAfterRestoration) {
    initialize();
    submit();
    g_frame_core.CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    g_frame_core.CCR = 0U;
    complete();
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    expect_settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, ForeignGateAndCacheOnRejectWithoutClaimingTheController) {
    EXPECT_CALL(hw, write(_, _)).Times(0);
    g_frame_rcc.AHB3ENR = RCC_AHB3ENR_DMA2DEN;
    EXPECT_EQ(nx_h743_frame_dma2d_initialize(&storage, &face, &config),
              NX_ERROR_BUSY);
    g_frame_rcc.AHB3ENR = 0U;
    g_frame_core.CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(nx_h743_frame_dma2d_initialize(&storage, &face, &config),
              NX_ERROR_UNSUPPORTED);
    g_frame_core.CCR = 0U;
    g_frame_debug.IDCODE = 0x10030450U;
    EXPECT_EQ(nx_h743_frame_dma2d_initialize(&storage, &face, &config),
              NX_ERROR_UNSUPPORTED);
    testing::Mock::VerifyAndClearExpectations(&hw);
    nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}

TEST_F(H743Frame, StopIsIdempotentAndDoesNotTouchASuccessorOwner) {
    initialize();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    nx_h743_frame_dma2d_storage_t successor_storage = {};
    const nx_frame_blitter_t successor = {&nx_h743_frame_dma2d_ops,
                                          &successor_storage};
    ASSERT_EQ(
        nx_h743_frame_dma2d_initialize(&successor_storage, &successor, &config),
        NX_SUCCESS);
    EXPECT_CALL(hw, read(_)).Times(0);
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    testing::Mock::VerifyAndClearExpectations(&hw);
    EXPECT_EQ(successor.ops->stop(successor.context), NX_SUCCESS);
    nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
    EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
}
} /* namespace */

extern "C" uint32_t nx_frame_model_read(volatile uint32_t* reg) {
    return hardware->read(reg);
}
extern "C" void nx_frame_model_write(volatile uint32_t* reg, uint32_t value) {
    hardware->write(reg, value);
}
extern "C" bool nx_frame_model_irq_quiet(void) {
    return hardware->irq_quiet;
}

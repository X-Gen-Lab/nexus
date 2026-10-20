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
#include <cstdlib>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
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
    bool lost_transfer_history = false;
    void write_register(volatile uint32_t* p, uint32_t value) {
        if (p == &g_frame_dma2d.IFCR) {
            g_frame_dma2d.ISR &= ~value;
        } else if (p == &g_frame_rcc.AHB3RSTR &&
                   (value & RCC_AHB3RSTR_DMA2DRST) != 0U) {
            lost_transfer_history = lost_transfer_history ||
                                    (g_frame_dma2d.CR & DMA2D_CR_START) != 0U;
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
            mmap(source, 0x80000U, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(mapping, MAP_FAILED);
        ASSERT_EQ(mapping, source);
        std::memset(mapping, 0, 0x80000U);
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
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
        hardware = nullptr;
    }
    void reframe(size_t row_bytes, size_t stride, size_t rows,
                 size_t capacity) {
        if (loan.owner != nullptr) {
            nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true,
                                          true};
            ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
        }
        slot.capacity = capacity;
        ASSERT_EQ(nx_stream_initialize(&stream, &slot, 1U), NX_SUCCESS);
        const nx_frame_plane_t plane = {0U, row_bytes, stride, rows};
        nx_frame_layout_t layout = {};
        ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
                  NX_SUCCESS);
        nx_stream_fill_t fill = {};
        ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
        ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    }
    void release_unsubmitted() {
        nx_dma_drain_facts_t proof = {loan.block.length, 0U, true, true, true};
        ASSERT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
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
        const auto pixels = g_frame_dma2d.NLR >> 16U;
        const bool pfc = (g_frame_dma2d.CR & DMA2D_CR_MODE) == DMA2D_CR_MODE_0;
        const auto input_pixel = pfc ? 4U : 2U;
        const auto width = pixels * 2U;
        const auto rows = g_frame_dma2d.NLR & 0xFFFFU;
        auto* src = reinterpret_cast<const uint8_t*>(
            static_cast<uintptr_t>(g_frame_dma2d.FGMAR));
        auto* dst = reinterpret_cast<uint8_t*>(
            static_cast<uintptr_t>(g_frame_dma2d.OMAR));
        for (unsigned row = 0U; row < rows; ++row) {
            if (pfc) {
                for (unsigned pixel = 0U; pixel < pixels; ++pixel) {
                    const auto* argb = src + pixel * 4U;
                    const auto rgb = static_cast<uint16_t>(
                        ((argb[2] >> 3U) << 11U) | ((argb[1] >> 2U) << 5U) |
                        (argb[0] >> 3U));
                    dst[pixel * 2U] = static_cast<uint8_t>(rgb & 255U);
                    dst[pixel * 2U + 1U] = static_cast<uint8_t>(rgb >> 8U);
                }
            } else {
                std::memcpy(dst, src, width);
            }
            src += (pixels + g_frame_dma2d.FGOR) * input_pixel;
            dst += width + g_frame_dma2d.OOR * 2U;
        }
        g_frame_dma2d.CR &=
            ~static_cast<uint32_t>(DMA2D_CR_START | DMA2D_CR_ABORT);
        g_frame_dma2d.ISR |= DMA2D_ISR_TCIF;
    }
    void expect_destructive_loss(bool asserted_reset) {
        const std::string previous = ::testing::FLAGS_gtest_death_test_style;
        ::testing::FLAGS_gtest_death_test_style = "threadsafe";
        ASSERT_EXIT(
            {
                if (nx_h743_frame_dma2d_initialize(&storage, &face, &config) !=
                    NX_SUCCESS)
                    std::_Exit(10);
                request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
                request.x = 0U;
                request.width_bytes = 8U;
                if (face.ops->submit(face.context, &request, 1U) != NX_SUCCESS)
                    std::_Exit(11);
                if (!asserted_reset)
                    g_frame_rcc.AHB3ENR = 0U;
                /* The external reset really erases START and register history.
                 * No independent observation of earlier bus-master access is
                 * supplied by this hardware fault model. */
                hw.write_register(&g_frame_rcc.AHB3RSTR, RCC_AHB3RSTR_DMA2DRST);
                if (!asserted_reset)
                    hw.write_register(&g_frame_rcc.AHB3RSTR, 0U);
                if (g_frame_dma2d.CR != 0U || !hw.lost_transfer_history)
                    std::_Exit(12);
                if (face.ops->service(face.context, 2U) != NX_ERROR_BUSY ||
                    nx_request_state(&request.base) != NX_REQUEST_QUARANTINED)
                    std::_Exit(13);
                g_frame_rcc.AHB3RSTR = 0U;
                g_frame_rcc.AHB3ENR = RCC_AHB3ENR_DMA2DEN;
                EXPECT_CALL(hw, read(_)).Times(0);
                EXPECT_CALL(hw, write(_, _)).Times(0);
                if (face.ops->service(face.context, 3U) != NX_ERROR_BUSY ||
                    nx_request_state(&request.base) != NX_REQUEST_QUARANTINED ||
                    slot.state != NX_STREAM_SLOT_BORROWED ||
                    loan.owner != &framed)
                    std::_Exit(17);
                if (face.ops->cancel(face.context, &request) != NX_SUCCESS ||
                    face.ops->stop(face.context) != NX_ERROR_BUSY ||
                    nx_request_prepare(&request.base, 200U) != NX_ERROR_STATE ||
                    nx_h743_frame_dma2d_initialize(&storage, &face, &config) !=
                        NX_ERROR_STATE)
                    std::_Exit(18);
                if (!testing::Mock::VerifyAndClearExpectations(&hw))
                    std::_Exit(19);
                /* A permanent borrow never returns into fixture reclamation. */
                std::_Exit(0);
            },
            testing::ExitedWithCode(0), "");
        ::testing::FLAGS_gtest_death_test_style = previous;
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

TEST_F(H743Frame, PfcUsesDistinctPixelUnitsAndIgnoresArgbAlpha) {
    initialize();
    request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    request.x = 0U;
    request.width_bytes = 8U;
    const uint8_t first[] = {0U, 0U, 255U, 0U, 0U, 255U, 0U, 255U};
    const uint8_t second[] = {255U, 0U, 0U, 127U, 255U, 255U, 255U, 1U};
    std::memcpy(source + 12U, first, sizeof(first));
    std::memcpy(source + 24U, second, sizeof(second));
    ASSERT_EQ(face.ops->submit(face.context, &request, 1U), NX_SUCCESS);
    EXPECT_EQ(g_frame_dma2d.CR, DMA2D_CR_MODE_0 | DMA2D_CR_START);
    EXPECT_EQ(g_frame_dma2d.FGMAR, 0x2400000CU);
    EXPECT_EQ(g_frame_dma2d.FGOR, 1U);
    EXPECT_EQ(g_frame_dma2d.OOR, 2U);
    EXPECT_EQ(g_frame_dma2d.NLR, (2U << 16U) | 2U);
    EXPECT_EQ(g_frame_dma2d.FGPFCCR, 0U);
    EXPECT_EQ(g_frame_dma2d.OPFCCR, 2U);
    complete();
    ASSERT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    expect_settled(NX_SUCCESS, 8U);
    const uint8_t output_first[] = {0U, 248U, 224U, 7U};
    const uint8_t output_second[] = {31U, 0U, 255U, 255U};
    EXPECT_EQ(std::memcmp(destination, output_first, sizeof(output_first)), 0);
    EXPECT_EQ(
        std::memcmp(destination + 8U, output_second, sizeof(output_second)), 0);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsUnknownFormatBeforeAdmissionOrRegisterEffects) {
    initialize();
    /* Malformed C API bytes avoid a C++ out-of-range enum conversion. */
    std::memset(&request.format, 0xff, sizeof(request.format));
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    if (nx_request_state(&request.base) == NX_REQUEST_ACTIVE) {
        EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
        EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    } else {
        nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
        EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    }
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, GuardRecoveryPreservesTheFirstObservedCancellation) {
    initialize();
    submit();
    hw.refuse_abort = true;
    ASSERT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    g_frame_core.CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    g_frame_core.CCR = 0U;
    g_frame_dma2d.CR = 0U;
    hw.refuse_abort = false;
    ASSERT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    expect_settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, PixelOffsetHighIgnoredBitsAreRejectedForExistingCopy) {
    initialize();
    request.destination = reinterpret_cast<uint8_t*>(0x24070000U);
    request.destination_stride = 0x4000U * 2U + request.width_bytes;
    request.destination_capacity =
        request.destination_stride + request.width_bytes;
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    if (nx_request_state(&request.base) == NX_REQUEST_ACTIVE) {
        EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
        EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    } else {
        nx_dma_drain_facts_t proof = {32U, 0U, true, true, true};
        EXPECT_EQ(nx_frame_dma_release(&framed, &loan, &proof), NX_SUCCESS);
    }
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, BothFormatsBoundEachPixelGapToFourteenBits) {
    initialize();
    for (const auto format :
         {NX_FRAME_BLIT_RGB565, NX_FRAME_BLIT_ARGB8888_TO_RGB565}) {
        const size_t input_pixel = format == NX_FRAME_BLIT_RGB565 ? 2U : 4U;
        for (const bool foreground : {false, true}) {
            for (const size_t gap : {0x3fffU, 0x4000U}) {
                SCOPED_TRACE(testing::Message()
                             << format << ',' << foreground << ',' << gap);
                const size_t width = input_pixel * 2U;
                const size_t source_stride =
                    width + (foreground ? gap * input_pixel : 0U);
                reframe(width, source_stride, 2U, source_stride + width);
                ASSERT_EQ(nx_request_prepare(&request.base, 100U), NX_SUCCESS);
                request.format = format;
                request.x = request.y = 0U;
                request.width_bytes = width;
                request.rows = 2U;
                request.destination = reinterpret_cast<uint8_t*>(0x24070000U);
                request.destination_stride = 4U + (foreground ? 0U : gap * 2U);
                request.destination_capacity = request.destination_stride + 4U;
                const auto expected =
                    gap == 0x3fffU ? NX_SUCCESS : NX_ERROR_INVALID;
                EXPECT_EQ(face.ops->submit(face.context, &request, 1U),
                          expected);
                if (nx_request_state(&request.base) != NX_REQUEST_READY) {
                    EXPECT_EQ(g_frame_dma2d.FGOR, foreground ? gap : 0U);
                    EXPECT_EQ(g_frame_dma2d.OOR, foreground ? 0U : gap);
                    EXPECT_EQ(face.ops->cancel(face.context, &request),
                              NX_SUCCESS);
                    EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
                }
            }
        }
    }
    if (loan.owner != nullptr)
        release_unsubmitted();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, PfcRejectsSourcePixelMisalignmentAndShortOutputBeforeWrites) {
    initialize();
    request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    request.x = 2U;
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.x = 0U;
    request.width_bytes = 6U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.width_bytes = 8U;
    request.destination_capacity = 11U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.destination_capacity = 12U;
    request.destination_stride = 3U;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    request.destination_stride = SIZE_MAX - 1U;
    request.destination_capacity = SIZE_MAX;
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    release_unsubmitted();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame,
       PfcRejectsMisalignedSourceStrideAndFullCapacityDestinationAlias) {
    initialize();
    reframe(8U, 10U, 3U, 64U);
    request.y = 0U;
    request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    request.x = 0U;
    request.width_bytes = 8U;
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    testing::Mock::VerifyAndClearExpectations(&hw);
    reframe(8U, 12U, 3U, 64U);
    request.destination = source + 48U;
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, &request, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    release_unsubmitted();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, PfcAbortPreservesModeAndNeverReassertsStart) {
    initialize();
    request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    request.x = 0U;
    request.width_bytes = 8U;
    ASSERT_EQ(face.ops->submit(face.context, &request, 1U), NX_SUCCESS);
    hw.refuse_abort = true;
    EXPECT_CALL(hw, write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_MODE_0 | DMA2D_CR_ABORT))
        .Times(1);
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_START)).Times(0);
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_MODE_0 | DMA2D_CR_START))
        .Times(0);
    ASSERT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(face.ops->stop(face.context), NX_ERROR_BUSY);
    nx_stream_fill_t fill = {};
    EXPECT_EQ(nx_stream_reserve(&stream, &fill), NX_ERROR_OVERFLOW);
    hw.refuse_abort = false;
    g_frame_dma2d.CR = DMA2D_CR_MODE_0;
    EXPECT_EQ(face.ops->service(face.context, 4U), NX_SUCCESS);
    expect_settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, PfcSetupReadbackFailureIsAcceptedAndNeverStarts) {
    initialize();
    request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    request.x = 0U;
    request.width_bytes = 8U;
    ON_CALL(hw, read(&g_frame_dma2d.FGPFCCR))
        .WillByDefault(testing::Return(2U));
    EXPECT_CALL(hw, write(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_MODE_0 | DMA2D_CR_START))
        .Times(0);
    EXPECT_CALL(hw, write(&g_frame_dma2d.CR, DMA2D_CR_MODE_0 | DMA2D_CR_ABORT))
        .Times(1);
    ASSERT_EQ(face.ops->submit(face.context, &request, 1U), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    expect_settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, PfcNoTcOrClutObservationCanSubstituteForFinalMasterDrain) {
    initialize();
    request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    request.x = 0U;
    request.width_bytes = 8U;
    submit();
    g_frame_dma2d.ISR = DMA2D_ISR_TCIF;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    hw.refuse_abort = true;
    EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    g_frame_dma2d.CR = DMA2D_CR_MODE_0;
    g_frame_dma2d.FGPFCCR = DMA2D_FGPFCCR_START;
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_ERROR_BUSY);
    EXPECT_EQ(slot.state, NX_STREAM_SLOT_BORROWED);
    g_frame_dma2d.FGPFCCR = 0U;
    EXPECT_EQ(face.ops->service(face.context, 4U), NX_SUCCESS);
    expect_settled(NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, GuardFailureObservedBeforeCancellationRemainsFirst) {
    initialize();
    submit();
    g_frame_core.CCR = SCB_CCR_DC_Msk;
    EXPECT_EQ(face.ops->service(face.context, 2U), NX_ERROR_BUSY);
    g_frame_core.CCR = 0U;
    EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    expect_settled(NX_ERROR_IO, 0U);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsRequestControlInTheDeclaredPoolCapacityTail) {
    initialize();
    reframe(8U, 12U, 3U, 512U);
    release_unsubmitted();
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    auto* alias = reinterpret_cast<nx_frame_blit_request_t*>(source + 128U);
    ::new (alias) nx_frame_blit_request_t(request);
    ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, alias, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&alias->base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    if (nx_request_state(&alias->base) != NX_REQUEST_READY) {
        EXPECT_EQ(face.ops->cancel(face.context, alias), NX_SUCCESS);
        EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    } else
        release_unsubmitted();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsOwnerControlInTheDeclaredPoolCapacityTail) {
    release_unsubmitted();
    auto* alias =
        reinterpret_cast<nx_h743_frame_dma2d_storage_t*>(source + 128U);
    ::new (alias) nx_h743_frame_dma2d_storage_t{};
    const nx_frame_blitter_t alias_face = {&nx_h743_frame_dma2d_ops, alias};
    ASSERT_EQ(nx_h743_frame_dma2d_initialize(alias, &alias_face, &config),
              NX_SUCCESS);
    reframe(8U, 12U, 3U, 512U);
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(alias_face.ops->submit(alias_face.context, &request, 1U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    if (nx_request_state(&request.base) != NX_REQUEST_READY) {
        EXPECT_EQ(alias_face.ops->cancel(alias_face.context, &request),
                  NX_SUCCESS);
        EXPECT_EQ(alias_face.ops->service(alias_face.context, 2U), NX_SUCCESS);
    } else
        release_unsubmitted();
    EXPECT_EQ(alias_face.ops->stop(alias_face.context), NX_SUCCESS);
}

TEST_F(H743Frame, RejectsRequestControlInAnotherFreePoolSlot) {
    initialize();
    release_unsubmitted();
    nx_stream_slot_t slots[2] = {};
    slots[0].data = source;
    slots[0].capacity = 64U;
    slots[1].data = source + 8192U;
    slots[1].capacity = 512U;
    ASSERT_EQ(nx_stream_initialize(&stream, slots, 2U), NX_SUCCESS);
    const nx_frame_plane_t plane = {0U, 8U, 12U, 3U};
    nx_frame_layout_t layout = {};
    ASSERT_EQ(nx_frame_layout_planar(&plane, 1U, &layout), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_initialize(&framed, &stream, &layout),
              NX_SUCCESS);
    auto* alias =
        reinterpret_cast<nx_frame_blit_request_t*>(slots[1].data + 128U);
    ::new (alias) nx_frame_blit_request_t(request);
    nx_stream_fill_t fill = {};
    ASSERT_EQ(nx_stream_reserve(&stream, &fill), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_publish(&framed, &fill, 0U), NX_SUCCESS);
    ASSERT_EQ(nx_frame_stream_acquire(&framed, &loan), NX_SUCCESS);
    EXPECT_CALL(hw, write(_, _)).Times(0);
    EXPECT_EQ(face.ops->submit(face.context, alias, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&alias->base), NX_REQUEST_READY);
    testing::Mock::VerifyAndClearExpectations(&hw);
    if (nx_request_state(&alias->base) != NX_REQUEST_READY) {
        EXPECT_EQ(face.ops->cancel(face.context, alias), NX_SUCCESS);
        EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    } else
        release_unsubmitted();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
}

TEST_F(H743Frame, PfcSettledRequestCanBecomeUnreadableBeforeFurtherService) {
    initialize();
    void* page = mmap(nullptr, 4096U, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(page, MAP_FAILED);
    auto* isolated = static_cast<nx_frame_blit_request_t*>(page);
    ::new (isolated) nx_frame_blit_request_t(request);
    isolated->format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
    isolated->x = 0U;
    isolated->width_bytes = 8U;
    ASSERT_EQ(face.ops->submit(face.context, isolated, 1U), NX_SUCCESS);
    complete();
    ASSERT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
    ASSERT_EQ(nx_request_state(&isolated->base), NX_REQUEST_SETTLED);
    ASSERT_EQ(mprotect(page, 4096U, PROT_NONE), 0);
    EXPECT_EQ(face.ops->service(face.context, 3U), NX_SUCCESS);
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
    EXPECT_EQ(munmap(page, 4096U), 0);
}

TEST_F(H743Frame, ObservedActiveResetPermanentlyQuarantinesErasedHistory) {
    expect_destructive_loss(true);
}

TEST_F(H743Frame, ObservedActiveGateLossNeverRecoversFromZeroedRegisters) {
    expect_destructive_loss(false);
}

TEST_F(H743Frame, PfcDimensionsUsePixelWidthAndSixteenBitLineCount) {
    initialize();
    for (const bool width_boundary : {false, true}) {
        for (const bool above : {false, true}) {
            SCOPED_TRACE(testing::Message() << width_boundary << ',' << above);
            const size_t pixels =
                width_boundary ? (above ? 0x4000U : 0x3fffU) : 1U;
            const size_t rows =
                width_boundary ? 1U : (above ? 0x10000U : 0xffffU);
            const size_t input_row = pixels * 4U;
            reframe(input_row, input_row, rows, input_row * rows);
            ASSERT_EQ(nx_request_prepare(&request.base, 100U), NX_SUCCESS);
            request.format = NX_FRAME_BLIT_ARGB8888_TO_RGB565;
            request.x = request.y = 0U;
            request.width_bytes = input_row;
            request.rows = rows;
            request.destination = reinterpret_cast<uint8_t*>(0x24040000U);
            request.destination_stride = pixels * 2U;
            request.destination_capacity = pixels * 2U * rows;
            EXPECT_EQ(face.ops->submit(face.context, &request, 1U),
                      above ? NX_ERROR_INVALID : NX_SUCCESS);
            if (nx_request_state(&request.base) != NX_REQUEST_READY) {
                EXPECT_EQ(g_frame_dma2d.NLR, (pixels << 16U) | rows);
                EXPECT_EQ(face.ops->cancel(face.context, &request), NX_SUCCESS);
                EXPECT_EQ(face.ops->service(face.context, 2U), NX_SUCCESS);
            }
        }
    }
    if (loan.owner != nullptr)
        release_unsubmitted();
    EXPECT_EQ(face.ops->stop(face.context), NX_SUCCESS);
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

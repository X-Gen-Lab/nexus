/**
 * \file            stm32_graphics_api_test.cpp
 * \brief           Public graphics extents, atomic preparation and dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/display.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
class GraphicsAPI : public testing::Test {
  protected:
    alignas(8) std::array<uint8_t, 128> source{}, destination{};
    nx_display_request_t display{};
    nx_image_request_t image{};

    void SetUp() override {
        nx_request_initialize(&display.base);
        nx_request_initialize(&image.base);
        source.fill(0x39);
        destination.fill(0x72);
    }
    nx_image_operation_t copy() {
        nx_image_operation_t op{};
        op.kind = NX_IMAGE_COPY;
        op.foreground = {source.data(), 64, 16, NX_IMAGE_RGB565};
        op.destination = {destination.data(), 64, 16, NX_IMAGE_RGB565};
        op.width = 8;
        op.height = 4;
        return op;
    }
    nx_display_operation_t frame() {
        nx_display_operation_t op{};
        op.frame = {source.data(), 16, 16, 4, 64};
        op.format = NX_IMAGE_RGB565;
        op.layer = 0;
        return op;
    }
};

TEST_F(GraphicsAPI, PreparationCopiesMetadataWithoutReadingOrBorrowingPixels) {
    auto op = frame();
    ASSERT_EQ(nx_display_prepare(&display, &op, NX_DEADLINE_NEVER), NX_SUCCESS);
    op.frame.data = nullptr;
    EXPECT_EQ(display.operation.frame.data, source.data());
    EXPECT_EQ(nx_request_state(&display.base), NX_REQUEST_READY);
    EXPECT_EQ(display.base.deadline, NX_DEADLINE_NEVER);
    const auto io = copy();
    ASSERT_EQ(nx_image_prepare(&image, &io, 100), NX_SUCCESS);
    EXPECT_EQ(image.operation.destination.data, destination.data());
    EXPECT_EQ(nx_request_state(&image.base), NX_REQUEST_READY);
    EXPECT_EQ(image.base.deadline, 100U);
    for (uint8_t value : destination)
        EXPECT_EQ(value, 0x72U);
}

TEST_F(GraphicsAPI, ImagePreparationRequiresAnExplicitFiniteBudget) {
    const auto before = image;
    const auto op = copy();
    EXPECT_EQ(nx_image_prepare(&image, &op, NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &image, sizeof(image)), 0);
}

TEST_F(GraphicsAPI, ImageRejectsAliasesIntoEntireRequestAndSourceDestination) {
    auto original = copy();
    const auto before = image;
    for (unsigned bad = 0; bad < 4; ++bad) {
        SCOPED_TRACE(bad);
        auto op = original;
        if (bad == 0)
            op.foreground.data = &image.base.state;
        if (bad == 1)
            op.destination.data = &image.operation;
        if (bad == 2)
            op.destination.data = source.data();
        if (bad == 3)
            op.destination.data = source.data() + 4;
        EXPECT_EQ(nx_image_prepare(&image, &op, 100), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&before, &image, sizeof(image)), 0);
    }
}

TEST_F(GraphicsAPI, EveryDeclaredCapacityParticipatesInAliasAdmission) {
    struct Storage {
        alignas(8) uint8_t payload[64];
        nx_image_request_t request;
    } storage{};
    nx_request_initialize(&storage.request.base);
    auto op = copy();
    op.foreground.data = storage.payload;
    op.foreground.capacity = sizeof(storage);
    const auto before = storage.request;
    EXPECT_EQ(nx_image_prepare(&storage.request, &op, 100), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &storage.request, sizeof(before)), 0);
}

TEST_F(GraphicsAPI,
       ImageArithmeticRejectsCompleteRowAndExtentOverflowAtomically) {
    const auto before = image;
    for (unsigned bad = 0; bad < 3; ++bad) {
        auto op = copy();
        if (bad == 0)
            op.width = std::numeric_limits<size_t>::max();
        if (bad == 1)
            op.height = std::numeric_limits<size_t>::max();
        if (bad == 2)
            op.destination.stride_bytes = std::numeric_limits<size_t>::max();
        EXPECT_EQ(nx_image_prepare(&image, &op, 100), NX_ERROR_EXHAUSTED);
        EXPECT_EQ(std::memcmp(&before, &image, sizeof(image)), 0);
    }
}

TEST_F(GraphicsAPI, ImageRejectsUnusedFieldsAndShortGeometryWithoutMutation) {
    const auto before = image;
    for (unsigned bad = 0; bad < 7; ++bad) {
        SCOPED_TRACE(bad);
        auto op = copy();
        if (bad == 0)
            op.background.data = source.data();
        if (bad == 1)
            op.foreground_alpha = 1;
        if (bad == 2)
            op.fill_value = 1;
        if (bad == 3)
            op.foreground.stride_bytes = 8;
        if (bad == 4)
            op.destination.capacity = 63;
        if (bad == 5)
            op.width = 0;
        if (bad == 6) {
            const unsigned invalid = 99;
            static_assert(sizeof(op.kind) == sizeof(invalid));
            std::memcpy(&op.kind, &invalid, sizeof(invalid));
        }
        EXPECT_EQ(nx_image_prepare(&image, &op, 100), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&before, &image, sizeof(image)), 0);
    }
}

TEST_F(GraphicsAPI, FillHasNoRetainedSourceAndHasExplicitDestinationEncoding) {
    auto op = copy();
    op.kind = NX_IMAGE_FILL;
    op.foreground = {};
    op.fill_value = 0xf800;
    ASSERT_EQ(nx_image_prepare(&image, &op, 100), NX_SUCCESS);
    EXPECT_EQ(image.operation.fill_value, 0xf800U);
    op.fill_value = 0x10000;
    const auto before = image;
    EXPECT_EQ(nx_image_prepare(&image, &op, 100), NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(&before, &image, sizeof(image)), 0);
}

TEST_F(GraphicsAPI, AlphaModesAreExplicitKnownAndUnusedOutsideBlend) {
    const auto before = image;
    for (unsigned bad = 0; bad < 4; ++bad) {
        SCOPED_TRACE(bad);
        auto op = copy();
        if (bad < 2) {
            if (bad == 0)
                op.foreground_alpha_mode = NX_IMAGE_ALPHA_REPLACE;
            else
                op.background_alpha_mode = NX_IMAGE_ALPHA_REPLACE;
        } else {
            op.kind = NX_IMAGE_BLEND;
            op.background = op.foreground;
            const unsigned invalid = 99;
            static_assert(sizeof(op.foreground_alpha_mode) == sizeof(invalid));
            void* field = bad == 2
                              ? static_cast<void*>(&op.foreground_alpha_mode)
                              : static_cast<void*>(&op.background_alpha_mode);
            std::memcpy(field, &invalid, sizeof(invalid));
        }
        EXPECT_EQ(nx_image_prepare(&image, &op, 100), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&before, &image, sizeof(image)), 0);
    }
}

TEST_F(GraphicsAPI, DisplayGeometryAndFullSpanAliasesRejectWithoutBorrowing) {
    const auto before = display;
    for (unsigned bad = 0; bad < 6; ++bad) {
        SCOPED_TRACE(bad);
        auto op = frame();
        if (bad == 0)
            op.frame.data = reinterpret_cast<const uint8_t*>(&display.base);
        if (bad == 1)
            op.frame.row_bytes = 0;
        if (bad == 2)
            op.frame.stride = 8;
        if (bad == 3)
            op.frame.rows = 0;
        if (bad == 4)
            op.frame.span_bytes = 63;
        if (bad == 5) {
            const unsigned invalid = 99;
            static_assert(sizeof(op.format) == sizeof(invalid));
            std::memcpy(&op.format, &invalid, sizeof(invalid));
        }
        EXPECT_EQ(nx_display_prepare(&display, &op, 100), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&before, &display, sizeof(display)), 0);
    }
    auto op = frame();
    op.frame.rows = std::numeric_limits<size_t>::max();
    EXPECT_EQ(nx_display_prepare(&display, &op, 100), NX_ERROR_EXHAUSTED);
}

TEST_F(GraphicsAPI, LiveRequestsCannotBePreparedAgainOrPartlyOverwritten) {
    auto op = frame();
    ASSERT_EQ(nx_display_prepare(&display, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&display.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    const auto before = display;
    EXPECT_EQ(nx_display_prepare(&display, &op, 200), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&before, &display, sizeof(display)), 0);
    auto io = copy();
    ASSERT_EQ(nx_image_prepare(&image, &io, 100), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&image.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    const auto ibefore = image;
    EXPECT_EQ(nx_image_prepare(&image, &io, 200), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&ibefore, &image, sizeof(image)), 0);
}

struct Backend {
    MOCK_METHOD(nx_result_t, display_submit, (nx_display_request_t*));
    MOCK_METHOD(nx_result_t, image_submit, (nx_image_request_t*));
    MOCK_METHOD(nx_result_t, display_query,
                (uint8_t, nx_display_constraints_t*));
    MOCK_METHOD(nx_result_t, display_snapshot,
                (uint8_t, nx_display_snapshot_t*));
};

TEST_F(GraphicsAPI,
       OneDisplayControllerDispatchesAnExplicitLayerWithoutAFactory) {
    Backend backend;
    nx_display_ops_t methods{};
    methods.query = [](void* context, uint8_t layer,
                       nx_display_constraints_t* out) {
        return static_cast<Backend*>(context)->display_query(layer, out);
    };
    methods.snapshot = [](void* context, uint8_t layer,
                          nx_display_snapshot_t* out) {
        return static_cast<Backend*>(context)->display_snapshot(layer, out);
    };
    const nx_display_port_t port{&methods, &backend};
    nx_display_constraints_t limits{};
    nx_display_snapshot_t observation{};
    EXPECT_CALL(backend, display_query(1, &limits))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_CALL(backend, display_snapshot(0, &observation))
        .WillOnce(testing::Return(NX_SUCCESS));
    EXPECT_EQ(nx_display_port_query(&port, 1, &limits), NX_SUCCESS);
    EXPECT_EQ(nx_display_port_snapshot(&port, 0, &observation), NX_SUCCESS);
}

TEST_F(GraphicsAPI, DispatchUsesTheExistingFaceContextExactlyOnce) {
    Backend backend;
    nx_display_ops_t dop{};
    dop.submit = [](void* context, nx_display_request_t* request) {
        return static_cast<Backend*>(context)->display_submit(request);
    };
    nx_image_ops_t iop{};
    iop.submit = [](void* context, nx_image_request_t* request) {
        return static_cast<Backend*>(context)->image_submit(request);
    };
    const nx_display_port_t dp{&dop, &backend};
    const nx_image_port_t ip{&iop, &backend};
    auto op = frame();
    auto io = copy();
    ASSERT_EQ(nx_display_prepare(&display, &op, 100), NX_SUCCESS);
    ASSERT_EQ(nx_image_prepare(&image, &io, 100), NX_SUCCESS);
    EXPECT_CALL(backend, display_submit(&display))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_CALL(backend, image_submit(&image))
        .WillOnce(testing::Return(NX_ERROR_IO));
    EXPECT_EQ(nx_display_port_submit(&dp, &display), NX_ERROR_BUSY);
    EXPECT_EQ(nx_image_port_submit(&ip, &image), NX_ERROR_IO);
    EXPECT_EQ(nx_request_state(&display.base), NX_REQUEST_READY);
    EXPECT_EQ(nx_request_state(&image.base), NX_REQUEST_READY);
}

TEST_F(GraphicsAPI, KnownFaceAndMethodStorageCannotBePayloadOrOutput) {
    Backend backend;
    nx_display_ops_t dop{};
    dop.submit = [](void* context, nx_display_request_t* request) {
        return static_cast<Backend*>(context)->display_submit(request);
    };
    nx_image_ops_t iop{};
    iop.submit = [](void* context, nx_image_request_t* request) {
        return static_cast<Backend*>(context)->image_submit(request);
    };
    const nx_display_port_t dp{&dop, &backend};
    const nx_image_port_t ip{&iop, &backend};
    auto op = frame();
    op.frame.data = reinterpret_cast<const uint8_t*>(&dop);
    ASSERT_EQ(nx_display_prepare(&display, &op, 100), NX_SUCCESS);
    auto io = copy();
    io.width = 4;
    io.height = 2;
    io.destination = {&iop, sizeof(iop), 16, NX_IMAGE_RGB565};
    ASSERT_EQ(nx_image_prepare(&image, &io, 100), NX_SUCCESS);
    EXPECT_CALL(backend, display_submit(testing::_)).Times(0);
    EXPECT_CALL(backend, image_submit(testing::_)).Times(0);
    EXPECT_EQ(nx_display_port_submit(&dp, &display), NX_ERROR_INVALID);
    EXPECT_EQ(nx_image_port_submit(&ip, &image), NX_ERROR_INVALID);
}

TEST_F(GraphicsAPI, UnsupportedMethodsAndNullFacesNeverCreateAnOwner) {
    int owner = 0;
    const nx_display_ops_t dop{};
    const nx_image_ops_t iop{};
    const nx_display_port_t dp{&dop, &owner};
    const nx_image_port_t ip{&iop, &owner};
    EXPECT_EQ(nx_display_port_service(&dp), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_image_port_service(&ip), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_display_port_stop(&dp), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_image_port_stop(&ip), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_display_port_stop(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_image_port_stop(nullptr), NX_ERROR_INVALID);
}
} /* namespace */

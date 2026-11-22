/**
 * \file            lvgl_test.cpp
 * \brief           Real LVGL renders within caller-owned bounded storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/lvgl.h"
#include "nexus/components/lvgl_profile.h"
#include <array>
#include <gtest/gtest.h>
#include <vector>

namespace {
class Gui : public testing::Test {
  protected:
    nx_lvgl_t gui{};
    std::vector<max_align_t> workspace;
    std::array<uint16_t, 64U * 8U> pixels{};
    size_t flushes{};
    size_t largest{};
    nx_time_us_t last_deadline{};
    nx_result_t flush_result{NX_SUCCESS};
    nx_result_t input_result{NX_SUCCESS};
    size_t input_reads{};
    bool try_reenter{};
    static nx_result_t flush(void* context, uint16_t, uint16_t, uint16_t width,
                             uint16_t height, const uint16_t* data,
                             size_t count, nx_time_us_t deadline) {
        auto& self = *static_cast<Gui*>(context);
        ++self.flushes;
        self.largest = std::max(self.largest, count);
        self.last_deadline = deadline;
        EXPECT_EQ(count, static_cast<size_t>(width) * height);
        EXPECT_GE(data, self.pixels.data());
        EXPECT_LE(data + count, self.pixels.data() + self.pixels.size());
        if (self.try_reenter) {
            EXPECT_EQ(nx_lvgl_stop(&self.gui), NX_ERROR_BUSY);
        }
        return self.flush_result;
    }
    static nx_result_t input(void* context, nx_lvgl_point_t* point,
                             nx_time_us_t deadline) {
        auto& self = *static_cast<Gui*>(context);
        ++self.input_reads;
        self.last_deadline = deadline;
        *point = {3U, 4U, true};
        return self.input_result;
    }
    nx_lvgl_config_t config{this, flush, nullptr,       nullptr,
                            64U,  64U,   pixels.data(), pixels.size()};
    void SetUp() override {
        const size_t bytes = nx_lvgl_workspace_bytes();
        workspace.resize((bytes + sizeof(max_align_t) - 1U) /
                         sizeof(max_align_t));
    }
    nx_result_t start() {
        return nx_lvgl_init(&gui, &config, workspace.data(),
                            workspace.size() * sizeof(max_align_t));
    }
    void TearDown() override {
        EXPECT_EQ(nx_lvgl_stop(&gui), NX_SUCCESS);
    }
};
TEST_F(Gui, SmallWorkspaceRejectsBeforePublishingAnOwner) {
    EXPECT_EQ(nx_lvgl_init(&gui, &config, workspace.data(),
                           nx_lvgl_workspace_bytes() - 1U),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(gui.private_state, nullptr);
    EXPECT_EQ(flushes, 0U);
    EXPECT_EQ(nx_lvgl_profile_display(&gui), nullptr);
}
TEST_F(Gui, ActualWidgetAndSoftwareRendererFlushOnlyBorrowedTiles) {
    ASSERT_EQ(start(), NX_SUCCESS);
    lv_display_t* display = nx_lvgl_profile_display(&gui);
    ASSERT_NE(display, nullptr);
    lv_obj_t* screen = lv_display_get_screen_active(display);
    ASSERT_NE(screen, nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x123456U), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t* label = lv_label_create(screen);
    ASSERT_NE(label, nullptr);
    lv_label_set_text(label, "Nexus");
    ASSERT_EQ(nx_lvgl_service(&gui, 40U, 1234U), NX_SUCCESS);
    EXPECT_GT(flushes, 0U);
    EXPECT_LE(largest, pixels.size());
    EXPECT_EQ(last_deadline, 1234U);
}
TEST_F(Gui, SelectedUpstreamGlobalOwnerRejectsASecondRuntime) {
    ASSERT_EQ(start(), NX_SUCCESS);
    nx_lvgl_t second{};
    std::vector<max_align_t> storage(workspace.size());
    EXPECT_EQ(nx_lvgl_init(&second, &config, storage.data(),
                           storage.size() * sizeof(max_align_t)),
              NX_ERROR_BUSY);
    EXPECT_EQ(second.private_state, nullptr);
}
TEST_F(Gui, FailedSynchronousFlushEndsPixelsAndClosesGuiProgress) {
    ASSERT_EQ(start(), NX_SUCCESS);
    flush_result = NX_ERROR_IO;
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 200U), NX_ERROR_IO);
    const size_t before = flushes;
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 300U), NX_ERROR_STATE);
    EXPECT_EQ(flushes, before);
    EXPECT_EQ(nx_lvgl_stop(&gui), NX_SUCCESS);
    EXPECT_EQ(gui.private_state, nullptr);
}
TEST_F(Gui, ReentrantStopCannotReclaimTheActiveRenderer) {
    ASSERT_EQ(start(), NX_SUCCESS);
    try_reenter = true;
    ASSERT_EQ(nx_lvgl_service(&gui, 40U, 200U), NX_SUCCESS);
    EXPECT_GT(flushes, 0U);
    EXPECT_NE(gui.private_state, nullptr);
}
TEST_F(Gui, StopEndsGlobalBorrowAndAllowsAnExplicitNewSession) {
    ASSERT_EQ(start(), NX_SUCCESS);
    EXPECT_EQ(nx_lvgl_stop(&gui), NX_SUCCESS);
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 200U), NX_ERROR_STATE);
    EXPECT_EQ(nx_lvgl_profile_display(&gui), nullptr);
    EXPECT_EQ(start(), NX_SUCCESS);
}
TEST_F(Gui, NativeRefreshOutsideAnAdmittedServiceCannotTouchThePanel) {
    ASSERT_EQ(start(), NX_SUCCESS);
    lv_refr_now(nx_lvgl_profile_display(&gui));
    EXPECT_EQ(flushes, 0U);
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 200U), NX_ERROR_STATE);
}
TEST_F(Gui, ActualPointerInputUsesTheSameDeadlineAndLatchesPortFailure) {
    config.input = input;
    config.input_context = this;
    ASSERT_EQ(start(), NX_SUCCESS);
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 123U), NX_SUCCESS);
    EXPECT_GT(input_reads, 0U);
    EXPECT_EQ(last_deadline, 123U);
    input_result = NX_ERROR_IO;
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 456U), NX_ERROR_IO);
    EXPECT_EQ(last_deadline, 456U);
    EXPECT_EQ(nx_lvgl_service(&gui, 40U, 789U), NX_ERROR_STATE);
}
} /* namespace */

/**
 * \file            lvgl.c
 * \brief           Explicit caller arena and synchronous real LVGL rendering
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/lvgl_profile.h"
#include "nexus/core/fault.h"
#include "nexus_lvgl_hooks.h"
#include "src/core/lv_global.h"
#include <string.h>

/** \brief One selected supplier global, pool and private connection record. */
typedef struct {
    lv_global_t global;
    _Alignas(8) uint8_t pool[LV_MEM_SIZE];
    nx_lvgl_config_t config;
    nx_lvgl_t* owner;
    lv_display_t* display;
    nx_lvgl_point_t point;
    nx_time_us_t deadline;
    nx_result_t error;
    bool in_call;
    bool dispatching;
} nx_lvgl_state_t;

/** \brief The selected upstream profile admits one application-owned GUI. */
static nx_lvgl_state_t* active;

/** \brief Numeric known-span checks precede writes to caller workspace. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    return pointer != NULL && (uintptr_t)pointer % alignment == 0U &&
           bytes <= UINTPTR_MAX - (uintptr_t)pointer;
}

/** \brief Checked known spans must not overwrite metadata or live objects. */
static bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return first <= second ? second - first < an : first - second < bn;
}

size_t nx_lvgl_workspace_bytes(void) {
    return sizeof(nx_lvgl_state_t);
}

size_t nx_lvgl_workspace_alignment(void) {
    return _Alignof(nx_lvgl_state_t);
}

lv_global_t* lv_global_default(void) {
    return active == NULL ? NULL : &active->global;
}

void* nx_lvgl_pool(size_t bytes) {
    return active != NULL && bytes == sizeof(active->pool) ? active->pool
                                                           : NULL;
}

_Noreturn void nx_lvgl_assert_fail(void) {
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

/** \brief Every settled callback return ends this tile's external access. */
static void flush(lv_display_t* display, const lv_area_t* area,
                  uint8_t* pixels) {
    nx_lvgl_state_t* state = lv_display_get_user_data(display);
    if (!state->dispatching)
        state->error = NX_ERROR_STATE;
    if (state->error == NX_SUCCESS) {
        uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
        uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);
        state->error = state->config.flush(
            state->config.display_context, (uint16_t)area->x1,
            (uint16_t)area->y1, width, height, (const uint16_t*)pixels,
            (size_t)width * height, state->deadline);
    }
    lv_display_flush_ready(display);
}

/** \brief EMPTY retains the last complete point; errors latch admission shut.
 */
static void input(lv_indev_t* device, lv_indev_data_t* data) {
    nx_lvgl_state_t* state = lv_indev_get_user_data(device);
    if (!state->dispatching)
        state->error = NX_ERROR_STATE;
    nx_lvgl_point_t point = state->point;
    if (state->error == NX_SUCCESS) {
        nx_result_t result = state->config.input(state->config.input_context,
                                                 &point, state->deadline);
        if (result == NX_SUCCESS &&
            (point.x >= state->config.width || point.y >= state->config.height))
            result = NX_ERROR_INVALID;
        if (result == NX_SUCCESS)
            state->point = point;
        else if (result != NX_ERROR_EMPTY)
            state->error = result;
    }
    data->point.x = state->point.x;
    data->point.y = state->point.y;
    data->state =
        state->point.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

nx_result_t nx_lvgl_init(nx_lvgl_t* gui, const nx_lvgl_config_t* config,
                         void* workspace, size_t bytes) {
    if (!span(gui, sizeof(*gui), _Alignof(nx_lvgl_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_lvgl_config_t)) ||
        !span(workspace, bytes, _Alignof(nx_lvgl_state_t)) ||
        config->flush == NULL || config->width == 0U || config->width > 320U ||
        config->height == 0U || config->height > 320U ||
        config->pixel_capacity < config->width ||
        config->pixel_capacity > UINT32_MAX / sizeof(uint16_t) ||
        !span(config->pixels, config->pixel_capacity * sizeof(uint16_t), 8U) ||
        (config->input == NULL && config->input_context != NULL) ||
        overlaps(gui, sizeof(*gui), config, sizeof(*config)) ||
        overlaps(workspace, bytes, gui, sizeof(*gui)) ||
        overlaps(workspace, bytes, config, sizeof(*config)) ||
        overlaps(config->pixels, config->pixel_capacity * sizeof(uint16_t),
                 workspace, bytes) ||
        overlaps(config->pixels, config->pixel_capacity * sizeof(uint16_t), gui,
                 sizeof(*gui)) ||
        overlaps(config->pixels, config->pixel_capacity * sizeof(uint16_t),
                 config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (bytes < sizeof(nx_lvgl_state_t))
        return NX_ERROR_EXHAUSTED;
    if (gui->private_state != NULL || active != NULL)
        return NX_ERROR_BUSY;
    nx_lvgl_state_t* state = workspace;
    memset(state, 0, sizeof(*state));
    state->config = *config;
    state->owner = gui;
    state->in_call = true;
    gui->private_state = state;
    active = state;
    lv_init();
    state->display = lv_display_create(config->width, config->height);
    if (state->display == NULL) {
        lv_deinit();
        gui->private_state = NULL;
        active = NULL;
        return NX_ERROR_EXHAUSTED;
    }
    lv_display_set_color_format(state->display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(state->display, state);
    lv_display_set_flush_cb(state->display, flush);
    lv_display_set_buffers(
        state->display, config->pixels, NULL,
        (uint32_t)(config->pixel_capacity * sizeof(uint16_t)),
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    if (config->input != NULL) {
        lv_indev_t* device = lv_indev_create();
        if (device == NULL) {
            lv_deinit();
            gui->private_state = NULL;
            active = NULL;
            return NX_ERROR_EXHAUSTED;
        }
        lv_indev_set_type(device, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(device, state->display);
        lv_indev_set_user_data(device, state);
        lv_indev_set_read_cb(device, input);
    }
    state->in_call = false;
    return NX_SUCCESS;
}

lv_display_t* nx_lvgl_profile_display(const nx_lvgl_t* gui) {
    return gui != NULL && active != NULL && active->owner == gui &&
                   gui->private_state == active && active->error == NX_SUCCESS
               ? active->display
               : NULL;
}

nx_result_t nx_lvgl_service(nx_lvgl_t* gui, uint32_t elapsed_ms,
                            nx_time_us_t deadline) {
    if (gui == NULL || elapsed_ms > 1000U || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (active == NULL || active->owner != gui || gui->private_state != active)
        return NX_ERROR_STATE;
    nx_lvgl_state_t* state = active;
    if (state->in_call)
        return NX_ERROR_BUSY;
    if (state->error != NX_SUCCESS)
        return NX_ERROR_STATE;
    state->in_call = true;
    state->deadline = deadline;
    lv_tick_inc(elapsed_ms);
    state->dispatching = true;
    (void)lv_timer_handler();
    state->dispatching = false;
    state->in_call = false;
    return state->error;
}

nx_result_t nx_lvgl_stop(nx_lvgl_t* gui) {
    if (gui == NULL)
        return NX_ERROR_INVALID;
    if (gui->private_state == NULL)
        return NX_SUCCESS;
    if (active == NULL || active->owner != gui || gui->private_state != active)
        return NX_ERROR_STATE;
    if (active->in_call)
        return NX_ERROR_BUSY;
    active->in_call = true;
    lv_deinit();
    gui->private_state = NULL;
    active = NULL;
    return NX_SUCCESS;
}

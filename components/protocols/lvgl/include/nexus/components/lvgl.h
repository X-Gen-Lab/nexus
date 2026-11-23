/**
 * \file            lvgl.h
 * \brief           Selected caller-arena LVGL RGB565 display and input bridge
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LVGL_H
#define NEXUS_COMPONENTS_LVGL_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Complete copied input sample in selected logical pixel coordinates.
 */
typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} nx_lvgl_point_t;

/**
 * \brief Explicit selected display, input and caller-owned tile storage.
 * \note One ordinary nonreentrant task advances LVGL, widgets and these ports.
 *       flush synchronously consumes the complete packed native RGB565 span;
 *       no access survives any return, including failure. input synchronously
 *       copies one point, EMPTY preserves the previous point. Both callbacks
 *       have a declared finite work bound and receive the current service's
 *       unchanged absolute deadline. Contexts remain live until successful
 *       stop. Optional input is NULL with NULL context. No OS lock or task is
 *       created. Width and height are1..320. Tile capacity is at least one
 *       logical row; alignment is reported by nx_lvgl_workspace_alignment.
 */
typedef struct {
    void* display_context;
    nx_result_t (*flush)(void*, uint16_t, uint16_t, uint16_t, uint16_t,
                         const uint16_t*, size_t, nx_time_us_t);
    void* input_context;
    nx_result_t (*input)(void*, nx_lvgl_point_t*, nx_time_us_t);
    uint16_t width;
    uint16_t height;
    uint16_t* pixels;
    size_t pixel_capacity;
} nx_lvgl_config_t;

/** \brief Opaque selected state; zero before first initialization. */
typedef struct {
    void* private_state;
} nx_lvgl_t;

/** \brief Exact profile bytes: private global state and32KiB bounded TLSF pool.
 */
size_t nx_lvgl_workspace_bytes(void);
/** \brief Required alignment for workspace and RGB565 tile storage. */
size_t nx_lvgl_workspace_alignment(void);

/**
 * \brief Bind one explicit selected GUI runtime without touching the display.
 * \param[in,out] gui: Zero/stopped exclusive object retained through stop.
 * \param[in] config: Call-owned copied settings, separate from all output.
 * \param[in,out] workspace: Aligned caller arena retained through stop.
 * \param[in] bytes: At least nx_lvgl_workspace_bytes, never a hidden heap.
 * \return SUCCESS, INVALID, EXHAUSTED before effects, or BUSY for a live owner.
 * \note The pinned upstream profile has one global GUI owner. This bridge
 *       locates its global state and allocator pool in the caller workspace.
 *       Pixels and opaque contexts remain live and disjoint from gui, config
 *       and workspace. Config is copied. No peripheral initialization or task
 *       occurs. Widget allocations use only the explicit32KiB arena. Supplier
 *       assertions/arena exhaustion report through nx_fault_observe and park;
 *       product UI budgets and factory checks remain necessary. Ordinary
 *       consumers see no supplier type; explicit UI code uses LVGLProfile.
 */
nx_result_t nx_lvgl_init(nx_lvgl_t* gui, const nx_lvgl_config_t* config,
                         void* workspace, size_t bytes);

/**
 * \brief Advance the selected real GUI once from an external elapsed budget.
 * \param[in,out] gui: Live same-task GUI owner and stable buffers.
 * \param[in] elapsed_ms: Explicit elapsed milliseconds, at most1000 per call.
 * \param[in] deadline: Finite absolute port-clock deadline for all callbacks.
 * \return SUCCESS, INVALID, STATE after a latched port failure, BUSY on
 *         reentry, or the first display/input callback failure.
 * \note Explicit elapsed time must belong to one admitted application epoch.
 *       Rendering/user events run synchronously; deadline is not a preemptive
 *       CPU WCET bound. No sleeping, hidden timer, worker or request retention.
 *       A failed settled flush ends its tile access, marks GUI failed and does
 *       not claim physical display completion. Join UI users and stop before
 *       recovering the separately owned uncertain device. No automatic retry.
 */
nx_result_t nx_lvgl_service(nx_lvgl_t* gui, uint32_t elapsed_ms,
                            nx_time_us_t deadline);

/**
 * \brief Delete selected GUI objects and release caller arena references.
 * \param[in,out] gui: Sole task after publishers/widget users have joined.
 * \return SUCCESS for completed/absent state, BUSY during callback/render,
 *         or INVALID/STATE for an incorrect owner.
 * \note Uses real lv_deinit; callbacks/context remain valid until it returns.
 *       Every external widget pointer becomes invalid. Stops no transport,
 *       panel, touch device or Platform; those owners retain their independent
 *       recovery responsibility. Reclaim arena/pixels only after SUCCESS.
 */
nx_result_t nx_lvgl_stop(nx_lvgl_t* gui);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_LVGL_H */

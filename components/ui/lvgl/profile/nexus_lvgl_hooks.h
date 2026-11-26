/**
 * \file            nexus_lvgl_hooks.h
 * \brief           Private bounded allocator and supplier assertion hooks
 * \author          Nexus Team
 */
#ifndef NEXUS_LVGL_HOOKS_H
#define NEXUS_LVGL_HOOKS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Supply the exact prebound pool during the unique lv_init call. */
void* nx_lvgl_pool(size_t bytes);
/** \brief Observe a terminal supplier invariant before retaining its effects.
 */
#ifdef __cplusplus
[[noreturn]] void nx_lvgl_assert_fail(void);
#else
_Noreturn void nx_lvgl_assert_fail(void);
#endif
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_LVGL_HOOKS_H */

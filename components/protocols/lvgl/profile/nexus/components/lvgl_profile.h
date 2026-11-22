/**
 * \file            lvgl_profile.h
 * \brief           Explicit supplier-facing UI composition for selected LVGL
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_LVGL_PROFILE_H
#define NEXUS_COMPONENTS_LVGL_PROFILE_H
#include "lvgl.h"
#include "nexus/components/lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Obtain the actual selected display in the existing single GUI owner.
 * \param[in] gui: Stable live owner, externally serialized with all UI code.
 * \return Borrowed supplier display, or NULL when stopped/failed/not owner.
 * \note Link LVGLProfile only in the UI implementation/assembly unit. Widgets,
 *       events and native pointers stay in that implementation and are joined
 *       before stop. This lookup allocates nothing and starts no peripheral.
 */
lv_display_t* nx_lvgl_profile_display(const nx_lvgl_t* gui);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_LVGL_PROFILE_H */

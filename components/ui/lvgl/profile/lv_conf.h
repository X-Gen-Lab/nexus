/**
 * \file            lv_conf.h
 * \brief           Fixed C11 RGB565 partial-render LVGL profile
 * \author          Nexus Team
 */
#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH                            16
#define LV_USE_OS                                 LV_OS_NONE
#define LV_USE_STDLIB_MALLOC                      LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING                      LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF                     LV_STDLIB_BUILTIN
#define LV_MEM_SIZE                               (32U * 1024U)
#define LV_MEM_POOL_EXPAND_SIZE                   0
#define LV_MEM_ADR                                0
#define LV_MEM_POOL_INCLUDE                       "nexus_lvgl_hooks.h"
#define LV_MEM_POOL_ALLOC                         nx_lvgl_pool
#define LV_ENABLE_GLOBAL_CUSTOM                   1
#define LV_USE_LOG                                0
#define LV_ASSERT_HANDLER_INCLUDE                 "nexus_lvgl_hooks.h"
#define LV_ASSERT_HANDLER                         nx_lvgl_assert_fail();
#define LV_DRAW_BUF_ALIGN                         8
#define LV_DRAW_BUF_STRIDE_ALIGN                  1
#define LV_DRAW_LAYER_MAX_MEMORY                  (4U * 1024U)
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE             (4U * 1024U)
#define LV_DRAW_SW_COMPLEX                        0
#define LV_DRAW_SW_DRAW_UNIT_CNT                  1
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE              0
#define LV_DRAW_SW_SUPPORT_RGB565                 1
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED         0
#define LV_DRAW_SW_SUPPORT_RGB565A8               0
#define LV_DRAW_SW_SUPPORT_RGB888                 0
#define LV_DRAW_SW_SUPPORT_XRGB8888               0
#define LV_DRAW_SW_SUPPORT_ARGB8888               0
#define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 0
#define LV_DRAW_SW_SUPPORT_L8                     0
#define LV_DRAW_SW_SUPPORT_AL88                   0
#define LV_DRAW_SW_SUPPORT_A8                     1
#define LV_DRAW_SW_SUPPORT_I1                     0
#define LV_CACHE_DEF_SIZE                         0
#define LV_IMAGE_HEADER_CACHE_DEF_CNT             0
#define LV_FONT_MONTSERRAT_14                     0
#define LV_FONT_UNSCII_8                          1
#define LV_FONT_DEFAULT                           &lv_font_unscii_8
#define LV_USE_THEME_DEFAULT                      0
#define LV_USE_THEME_SIMPLE                       0
#define LV_USE_THEME_MONO                         0
#define LV_USE_FLEX                               0
#define LV_USE_GRID                               0
#define LV_USE_ANIMIMG                            0
#define LV_USE_ARC                                0
#define LV_USE_BAR                                0
#define LV_USE_BUTTON                             1
#define LV_USE_BUTTONMATRIX                       0
#define LV_USE_CALENDAR                           0
#define LV_USE_CANVAS                             0
#define LV_USE_CHART                              0
#define LV_USE_CHECKBOX                           0
#define LV_USE_DROPDOWN                           0
#define LV_USE_IMAGE                              0
#define LV_USE_IMAGEBUTTON                        0
#define LV_USE_KEYBOARD                           0
#define LV_USE_LABEL                              1
#define LV_LABEL_TEXT_SELECTION                   0
#define LV_USE_LED                                0
#define LV_USE_LINE                               0
#define LV_USE_LIST                               0
#define LV_USE_MENU                               0
#define LV_USE_MSGBOX                             0
#define LV_USE_ROLLER                             0
#define LV_USE_SCALE                              0
#define LV_USE_SLIDER                             0
#define LV_USE_SPAN                               0
#define LV_USE_SPINBOX                            0
#define LV_USE_SPINNER                            0
#define LV_USE_SWITCH                             0
#define LV_USE_TABLE                              0
#define LV_USE_TABVIEW                            0
#define LV_USE_TEXTAREA                           0
#define LV_USE_TILEVIEW                           0
#define LV_USE_WIN                                0
#define LV_USE_RLE                                0
#define LV_USE_SYSMON                             0
#define LV_USE_OBSERVER                           0
#define LV_USE_XML                                0
#define LV_BUILD_EXAMPLES                         0
#define LV_BUILD_DEMOS                            0
#endif /* LV_CONF_H */

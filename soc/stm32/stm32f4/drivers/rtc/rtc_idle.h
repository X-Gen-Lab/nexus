/**
 * \file            rtc_idle.h
 * \brief           Private same-owner RTC bridge for one masked STOP window
 * \author          Nexus Team
 */
#ifndef NX_STM32_RTC_IDLE_H
#define NX_STM32_RTC_IDLE_H
#include "nexus/io/rtc.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Sample nominal 256 Hz ticks within the current calendar day.
 * \param[in]       port: Existing F4 RTC face and its live suspended owner.
 * \param[out]      ticks_256: Separate aligned output, unchanged on failure.
 * \return          SUCCESS, INVALID, CONTEXT, STATE or BUSY for unstable reads.
 * \note            Privileged Thread, PRIMASK=1, BASEPRI=FAULTMASK=0.
 *                  Only WUT event route22, RTCCLK/16 and reload255 are allowed.
 *                  Requires PRER127/255 and no calibration or other ownership.
 *                  Reuses ordinary calendar BCD and stable SSR/TR/DR rules.
 *                  The count is modulo86400*256; this is not 32768 Hz accuracy.
 *                  Caller proves a window below24 h and applies drift bounds.
 *                  System is live before and after STOP; Core time is separate.
 *                  No clock read, register write, owner change or wait.
 */
nx_result_t nx_stm32_rtc_idle_sample(const nx_rtc_port_t* port,
                                     uint32_t* ticks_256);
/**
 * \brief           Explicitly acknowledge only owned WUTF and EXTI22 pending.
 * \param[in]       port: Same suspended face and exact masked execution scope.
 * \return          SUCCESS, INVALID, CONTEXT, STATE or IO after partial clear.
 * \note            Same fixed profile as sample. Observed foreign pending
 *                  events reject before writes. RTC W0C and EXTI W1C preserve
 *                  every other flag. A new event or refused readback reports
 *                  IO; partial owned acknowledgement may remain, while foreign
 *                  events and suspension remain observable. No retry loop,
 *                  calendar/backup mutation, output restart or owner release.
 *                  WUT flags coalesce: this never counts physical wake edges.
 */
nx_result_t nx_stm32_rtc_idle_ack(const nx_rtc_port_t* port);
#ifdef __cplusplus
}
#endif
#endif

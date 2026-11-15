/**
 * \file            rtc_lptim_model.h
 * \brief           Explicit host source progression and withdrawal faults
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_RTC_LPTIM_MODEL_H
#define NEXUS_NATIVE_RTC_LPTIM_MODEL_H
#include "nexus/io/lptim.h"
#include "nexus/io/rtc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Advance a set raw calendar by bounded explicit source seconds.
 * \param[in] port: Exact Native RTC face, serialized with every public call.
 * \param[in] seconds: 0..86400; zero never creates an event.
 * \return SUCCESS, STATE if unset/unconstructed, INVALID or UNSUPPORTED.
 * \note Constant bounded work, at most one date carry. A closed set calendar
 *       may advance; this does not reopen admission or prove backup retention.
 *       Does not read/advance Core clock, publish, settle or change terminal
 *       cause. Source changes are not an IRQ, worker or hardware qualification.
 */
nx_result_t nx_native_rtc_model_advance(const nx_rtc_port_t* port,
                                        uint32_t seconds);
/** \brief Inject/remove the external inability to withdraw an armed source.
 * \param[in] port: Exact Native RTC face on the same serialized owner.
 * \param[in] hold: True prevents withdrawal; false only removes the fault.
 * \return SUCCESS or INVALID/STATE/UNSUPPORTED; never settles a request.
 */
nx_result_t nx_native_rtc_model_hold_withdraw(const nx_rtc_port_t* port,
                                              bool hold);
/** \brief Advance a finite source, saturating at its exact shot limit.
 * \param[in] port: Exact Native LPTIM face, serialized with all public calls.
 * \param[in] ticks: 0..65536 source ticks; idle/unarmed does not accumulate.
 * \return SUCCESS or INVALID/STATE/UNSUPPORTED.
 * \note At full65536 the sampled 16-bit counter is zero; pending completion is
 *       independent. Held withdrawal permits final progress beyond cancel/time
 *       observation. Does not touch the request, terminal cause or Core clock.
 */
nx_result_t nx_native_lptim_model_advance(const nx_lptim_port_t* port,
                                          uint32_t ticks);
/** \brief Inject/remove withdrawal hold; real public service still drains.
 * \param[in] port: Exact Native LPTIM face on the serialized executor.
 * \param[in] hold: False does not clear provider references or publish results.
 * \return SUCCESS or INVALID/STATE/UNSUPPORTED.
 */
nx_result_t nx_native_lptim_model_hold_withdraw(const nx_lptim_port_t* port,
                                                bool hold);
#ifdef __cplusplus
}
#endif
#endif

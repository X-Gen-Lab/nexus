/**
 * \file            gd32f470_sleep.h
 * \brief           Fixed RUN profile and explicit drained shallow sleep
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_GD32F470_SLEEP_H
#define NEXUS_GD32F470_SLEEP_H
#include "nexus/components/clock_domain.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Execute one shallow WFI after final drain/clock/wake checks
 * \return          SUCCESS with slept=false for an already changed sequence,
 *                  BUSY for retained consumers, STATE for stale control,
 *                  UNSUPPORTED for STOP/dynamic or a nonfixed RUN profile,
 *                  INVALID or CONTEXT before any WFI or hardware change
 * \note            Serialized privileged Thread with incoming masks clear.
 *                  Owner must enroll every relevant clock consumer, keep the
 *                  domain closed, exclude other RCC owners/NMI clock changes
 *                  and establish a live enabled configurable wake IRQ. Same
 *                  CPU Thread/IRQ sequence publishers; no DMA/SMP/NMI writers.
 *                  Does not change RCC, SCR, Tick, deadlines or clock domain.
 *                  Wake can be spurious: retry the authoritative predicate.
 *                  Sequence and output must not alias provider/lease storage
 *                  or each other. Their storage remains live through return.
 *                  Exact clock readback is not voltage/electrical/HIL proof.
 */
nx_result_t nx_gd32f470_sleep(nx_clock_barrier_t barrier,
                              nx_clock_power_mode_t mode,
                              const uint32_t* wake_sequence, uint32_t expected,
                              bool* slept);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_GD32F470_SLEEP_H */

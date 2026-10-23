/**
 * \file            stm32f407_handoff.h
 * \brief           Exact F407 Flash-image transfer after explicit owner drain
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_STM32F407_HANDOFF_H
#define NEXUS_COMPONENTS_STM32F407_HANDOFF_H
#include "nexus/components/clock_domain.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Construct the single static F407 handoff admission domain
 * \param[in]       consumer_capacity: Positive bound on enrolled live tokens.
 * \return          SUCCESS, INVALID, CONTEXT or STATE; never resets an epoch.
 * \note            Serialized privileged Thread, once before any consumer.
 *                  No heap, hardware writes, discovery or implicit startup.
 *                  The caller enrolls every platform/product execution, IRQ,
 *                  DMA, observer and memory owner that can outlive entry.
 *                  Release a lease only after its actual join/drain.
 */
nx_result_t nx_stm32f407_handoff_initialize(size_t consumer_capacity);
/**
 * \brief           Return the existing single static admission domain
 * \return          Stable object identity, including before initialization.
 * \note            Pure lookup; does not initialize, acquire or touch hardware.
 */
nx_clock_domain_t* nx_stm32f407_handoff_domain(void);
/**
 * \brief           Validate stopped F407 state and tail-transfer to Flash
 * \param[in]       owners: Closed, drained barrier from this exact domain.
 * \param[in]       vector_address: 512-aligned physical Flash vector address.
 * \return          Error before transfer; success never returns.
 * \note            Selected STM32F407VET6/ZGT6 baremetal images only.
 *                  Caller is privileged Thread on MSP, PRIMASK=1, other masks
 *                  clear. No scheduler may have started. Stop the actual
 *                  Platform, join all users and keep admission closed.
 *                  An unrelated empty domain cannot authorize transfer.
 *                  Rejects active FP/lazy preservation, partial CP10/11
 *                  permissions, MPU, ART, SysTick, enabled/pending/active IRQs,
 *                  non-HSI16/reset buses, peripheral/DMA gates, busy Flash
 *                  and retained System effects. Ordinary inactive CP10/11
 *                  permissions from SystemInit are supported.
 *                  Main SRAM is preserved; MSP is 8-aligned in
 *                  (0x20000000,0x20020000]. All 98 vectors fit exact observed
 *                  and generated Flash geometry. Reset PC is Thumb with its
 *                  first halfword in Flash. Success sets VTOR, disables
 *                  CP10/11, sets CONTROL=0, PSP=0 and MSP, then branches to PC
 *                  with PRIMASK=1. This is a masked handoff entry.
 *                  No RAM copy/clear, image authentication/selection, device
 *                  drain, clock change or physical qualification occurs.
 *                  Target owns RAM startup and resets controllers before
 *                  enabling gates. No old deadline survives transfer.
 *                  Exclude NMI/HardFault, debugger, DMA and other bus masters.
 *                  Enrollment is caller policy; Platform and product owners
 *                  are never enrolled automatically.
 *                  Rejection preserves masks and performs no writable MMIO.
 *                  Reading SysTick CTRL consumes COUNTFLAG even if rejected.
 */
nx_result_t nx_stm32f407_handoff(nx_clock_barrier_t owners,
                                 uint32_t vector_address);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_STM32F407_HANDOFF_H */

/**
 * \file            stm32u575_security.h
 * \brief           Private cold-owner U575 single-thread dual-world mechanisms
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_SECURITY_H
#define NX_STM32U575_SECURITY_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Integrator-owned partition, independent of the existing TZEN0 profile
 * \note All spans are exact, positive base/byte extents. NonSecure Flash is a
 *       complete-page subset of bank2; SRAM is a complete-superblock subset of
 *       SRAM3 with at least one remaining Secure superblock. NSC resides in
 *       Secure bank1 outside its first vector page. The Secure stack is wholly
 *       within SRAM1's Secure alias. The linker and boot owner exclusively own
 *       these spans; this interface never chooses a product partition.
 */
typedef struct {
    uint32_t ns_flash_base;
    uint32_t ns_flash_bytes;
    uint32_t ns_ram_base;
    uint32_t ns_ram_bytes;
    uint32_t nsc_base;
    uint32_t nsc_bytes;
    uint32_t secure_stack_base;
    uint32_t secure_stack_bytes;
} nx_stm32u575_tz_layout_t;
/** \brief A failed cold transition has no in-place retry or release operation.
 */
typedef enum {
    NX_STM32U575_TZ_COLD = 0,
    NX_STM32U575_TZ_FAILED,
    NX_STM32U575_TZ_READY,
    NX_STM32U575_TZ_RUNNING
} nx_stm32u575_tz_state_t;
/**
 * \brief Secure-owned static lifecycle storage, never passed by an NS caller
 * \note Zero initialize before cold ownership. Keep in Secure SRAM1. READY or
 *       RUNNING layout is immutable until reset. FAILED retains every possible
 *       effect; it cannot authorize a NonSecure entry or generic retry.
 */
typedef struct {
    nx_stm32u575_tz_layout_t layout;
    nx_stm32u575_tz_state_t state;
    bool effects;
} nx_stm32u575_tz_t;
/**
 * \brief Validate exact layout, boot options and CPU context without writes
 * \return SUCCESS or INVALID/CONTEXT/BUSY/UNSUPPORTED; no owner updates or
 *         configuration writes. Hardware observation may clear read flags.
 * \note Secure privileged Thread, PRIMASK=1, no live NonSecure execution,
 *       interrupts, DMA, caches or bootloader ownership. Option-byte identity
 *       is observed, never programmed. A station must establish the Secure
 *       boot contract before executing this image; a late read cannot fix it.
 */
nx_result_t nx_stm32u575_tz_preflight(const nx_stm32u575_tz_t* owner,
                                      const nx_stm32u575_tz_layout_t* layout);
/**
 * \brief Prepare explicit SAU, NS MPU and locked SRAM partition once at boot
 * \return SUCCESS publishes READY; errors after acquisition retain FAILED and
 *         effects=true, requiring reset without entering NonSecure software.
 * \note Same exclusive masked Secure owner as preflight. Clears the exact
 *       future NS SRAM before declassification. No Flash erase/program or
 *       option-byte writes; rejects incompatible preprovisioned NS Flash.
 *       NonSecure has no peripheral region and no privileged exception entry.
 */
nx_result_t nx_stm32u575_tz_prepare(nx_stm32u575_tz_t* owner,
                                    const nx_stm32u575_tz_layout_t* layout);
/**
 * \brief Revalidate the NS vector, stack and entry against the sealed layout
 * \return SUCCESS copies two scalar words to trusted Secure outputs; errors
 *         leave both outputs unchanged and do not transfer execution.
 * \note READY exclusive masked Secure owner only. All 140 exception vectors
 *       after MSP/Reset must be zero. No NS kernel, IRQ, callback or privileged
 *       handler is permitted. Mutable untrusted vectors cannot establish an
 *       immutable NS MPU boundary. Firmware/loader authentication is external.
 */
nx_result_t nx_stm32u575_tz_entry(const nx_stm32u575_tz_t* owner,
                                  uint32_t* stack, uint32_t* entry);
/**
 * \brief Enter the prepared NonSecure image as unprivileged Thread
 * \return Errors reject before transition. A valid entry never returns.
 * \note Performs final entry validation, sets NS VTOR/MSP/MSPLIM/CONTROL and
 *       masks, publishes RUNNING, clears the Secure bootstrap PRIMASK, and uses
 *       the compiler's actual CMSE NonSecure call ABI. An unexpected NS return
 *       is terminal. Secure fault/NMI handlers must fail stop, never mutate
 *       attribution or dispatch another gateway. No asynchronous user loans.
 */
nx_result_t nx_stm32u575_tz_launch(nx_stm32u575_tz_t* owner);
#ifdef __cplusplus
}
#endif
#endif /* NX_STM32U575_SECURITY_H */

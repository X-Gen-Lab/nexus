/* Production HAL lifecycle, with counted platform hooks and modeled CPU masks.
 * The real-Arch variant tests Native nesting; neither fixture is MCU HIL.
 */
#include "arch/nx_arch.h"
#include "hal/nx_hal.h"
#include <assert.h>
#include <stdio.h>

static unsigned init_calls, deinit_calls;
static uintptr_t fence_owner;
bool osal_is_initialized(void) {
    return false;
}
nx_status_t nx_device_shutdown_check(void) {
    return NX_OK;
}
nx_status_t nx_device_shutdown_begin_owned(uintptr_t owner) {
    assert(fence_owner == 0);
    fence_owner = owner;
    return NX_OK;
}
nx_status_t nx_device_shutdown_quarantine_begin(uintptr_t owner) {
    return nx_device_shutdown_begin_owned(owner);
}
nx_status_t nx_device_shutdown_end_owned(uintptr_t owner) {
    assert(fence_owner == owner);
    fence_owner = 0;
    return NX_OK;
}
static nx_status_t init_status = NX_OK, deinit_status = NX_OK;

nx_status_t nx_platform_init(void) {
    ++init_calls;
    return init_status;
}

nx_status_t nx_platform_deinit(void) {
    ++deinit_calls;
    return deinit_status;
}

#if !defined(NX_HAL_CONTEXT_REAL_ARCH)
static bool in_isr;
static uint32_t primask, basepri, faultmask;

bool nx_arch_in_isr(void) {
    return in_isr;
}

bool nx_arch_irq_is_masked(void) {
    return (primask & 1u) != 0 || basepri != 0 || (faultmask & 1u) != 0;
}
#endif

static void reject_context(bool initialized, nx_status_t expected) {
    unsigned init_before = init_calls, deinit_before = deinit_calls;
    assert(nx_hal_init() == expected);
    assert(nx_hal_deinit() == expected);
    assert(nx_hal_is_initialized() == initialized);
    assert(init_calls == init_before && deinit_calls == deinit_before);
}

static void reject_all_masks(bool initialized) {
#if defined(NX_HAL_CONTEXT_REAL_ARCH)
    assert(!nx_arch_irq_is_masked());
    nx_arch_irq_state_t outer = nx_arch_irq_save();
    nx_arch_irq_state_t inner = nx_arch_irq_save();
    assert(nx_arch_irq_is_masked());
    reject_context(initialized, NX_ERR_INVALID_STATE);
    assert(nx_arch_irq_is_masked());
    nx_arch_irq_restore(inner);
    assert(nx_arch_irq_is_masked());
    reject_context(initialized, NX_ERR_INVALID_STATE);
    nx_arch_irq_restore(outer);
    assert(!nx_arch_irq_is_masked());
#else
    in_isr = true;
    reject_context(initialized, NX_ERR_CONTEXT);
    assert(in_isr);
    in_isr = false;
    for (unsigned mask = 0; mask < 3; ++mask) {
        primask = mask == 0 ? 1u : 0u;
        basepri = mask == 1 ? 0x80u : 0u;
        faultmask = mask == 2 ? 1u : 0u;
        reject_context(initialized, NX_ERR_INVALID_STATE);
        assert(primask == (mask == 0 ? 1u : 0u));
        assert(basepri == (mask == 1 ? 0x80u : 0u));
        assert(faultmask == (mask == 2 ? 1u : 0u));
    }
    primask = basepri = faultmask = 0;
#endif
}

int main(void) {
    assert(!nx_hal_is_initialized());
    reject_all_masks(false);
    assert(nx_hal_deinit() == NX_OK);
    assert(init_calls == 0 && deinit_calls == 0);

    init_status = NX_ERR_TIMEOUT;
    assert(nx_hal_init() == NX_ERR_TIMEOUT);
    assert(!nx_hal_is_initialized() && init_calls == 1 && deinit_calls == 1);
    init_status = NX_OK;
    assert(nx_hal_init() == NX_OK);
    assert(nx_hal_is_initialized() && init_calls == 2);
    assert(nx_hal_init() == NX_OK && init_calls == 2);
    reject_all_masks(true);

    deinit_status = NX_ERR_BUSY;
    assert(nx_hal_deinit() == NX_ERR_BUSY);
    assert(nx_hal_get_state() == NX_HAL_PARTIAL && deinit_calls == 2);
    assert(fence_owner != 0);
    assert(nx_hal_init() == NX_ERR_INVALID_STATE);
    reject_all_masks(false);
    deinit_status = NX_OK;
    assert(nx_hal_deinit() == NX_OK);
    assert(!nx_hal_is_initialized() && deinit_calls == 3 && fence_owner == 0);
    reject_all_masks(false);
    assert(nx_hal_deinit() == NX_OK && deinit_calls == 3);
    puts("Production HAL context rejection preserves masks, state and platform hooks");
    return 0;
}

"""One optional fixed-run provider lifecycle lease; no clock-policy engine."""
from tools.configure.providers.common import fail


def resolve(value, result):
    """Optional lifecycle policy belongs to the selected concrete SoC."""
    from tools.configure.providers import maintained
    resolve_selected = getattr(maintained(result["soc_family"]),
                               "resolve_runtime_power", None)
    if resolve_selected is None:
        fail("power has no maintained lifecycle for the selected SoC")
    return resolve_selected(value, result)


def header():
    return [
        '#include "nexus/components/clock_domain.h"',
        '/** \\brief Pure identity lookup; unused until Platform starts.',
        ' * Only the serialized lifecycle owner may close/reopen this domain.',
        ' * It certifies this selected SPI lifecycle, never System or TIM2. */',
        'nx_clock_domain_t* nx_platform_runtime_domain(void);',
        '/** \\brief Close admission and join the selected I/O; keep TIM2 live.',
        ' * Privileged unmasked Thread, exclusive lifecycle owner only.',
        ' * Join submitters first. BUSY requires real cancel/service and retry;',
        ' * close alone does not gate already initialized endpoint submission.',
        ' * Failed DMA/IRQ/CS/pad/gate withdrawal retains the lifecycle lease.',
        ' * Request SETTLED does not release this provider lease. */',
        'nx_result_t nx_platform_runtime_pause(void);',
        '/** \\brief Reopen and restore the same selected I/O with live TIM2.',
        ' * Same serialization as pause; no DVFS, STOP, WFI or new time epoch.',
        ' * A partial failure retains effects until pause/global stop succeeds.',
        ' * Returns original primary and cleanup status separately. */',
        'nx_platform_start_result_t nx_platform_runtime_resume(void);']


def state():
    return ['#include "clock_domain_impl.h"',
            'static nx_clock_domain_t s_nx_runtime_domain;',
            'static nx_clock_lease_t s_nx_runtime_lease;',
            'nx_clock_domain_t* nx_platform_runtime_domain(void) {',
            '    return &s_nx_runtime_domain;', '}',
            '/** \\brief Acquire before selected peripheral resource effects. */',
            'static nx_result_t nx_runtime_acquire(void) {',
            '    if (!s_nx_runtime_domain.initialized) {',
            '        nx_result_t status = nx_clock_domain_construct(&s_nx_runtime_domain, 1u);',
            '        if (status != NX_SUCCESS) { return status; }',
            '    }',
            '    return nx_clock_domain_acquire(&s_nx_runtime_domain, &s_nx_runtime_lease);', '}',
            '/** \\brief Close metadata only; never revoke an existing borrow. */',
            'static nx_result_t nx_runtime_close(void) {',
            '    if (!s_nx_runtime_domain.initialized) { return NX_SUCCESS; }',
            '    nx_clock_barrier_t barrier;',
            '    return nx_clock_domain_close(&s_nx_runtime_domain, &barrier);', '}',
            '/** \\brief Called only after actual controller, pad and gate drain. */',
            'static nx_result_t nx_runtime_release(void) {',
            '    if (!s_nx_runtime_domain.initialized) { return NX_SUCCESS; }',
            '    if (s_nx_runtime_lease.owner != NULL) {',
            '        nx_result_t status = nx_clock_domain_release(&s_nx_runtime_domain, &s_nx_runtime_lease);',
            '        if (status != NX_SUCCESS) { return status; }',
            '    }',
            '    nx_clock_barrier_t barrier;',
            '    nx_result_t status = nx_clock_domain_close(&s_nx_runtime_domain, &barrier);',
            '    return status == NX_SUCCESS ? nx_clock_domain_quiescent(barrier) : status;', '}',
            '/** \\brief Epoch advance is explicit, drained and nonwrapping. */',
            'static nx_result_t nx_runtime_reopen(void) {',
            '    nx_clock_barrier_t barrier;',
            '    nx_result_t status = nx_clock_domain_close(&s_nx_runtime_domain, &barrier);',
            '    return status == NX_SUCCESS ? nx_clock_domain_reopen(barrier) : status;', '}']


def runtime_functions(result, context_guard, initialize):
    """Use the selected SoC's existing cold start and cleanup helpers."""
    from tools.configure.providers import maintained
    return maintained(result.soc_family).runtime_power_functions(
        result, context_guard, initialize)

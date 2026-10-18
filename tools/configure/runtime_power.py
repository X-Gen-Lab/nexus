"""One optional fixed-run provider lifecycle lease; no clock-policy engine."""
try:
    from .providers.common import fail
    from .ir import RuntimePowerIR
except ImportError:
    from providers.common import fail
    from ir import RuntimePowerIR


def resolve(value, result):
    """Admit only the explicit single SPI4 finite-DMA software reference."""
    if (not isinstance(value, dict) or set(value) != {'mode', 'controllers'} or
            value['mode'] != 'fixed-run' or
            value['controllers'] != ['bus4']):
        fail('power requires fixed-run and exactly controllers=["bus4"]')
    controllers = result['controllers']
    if (result['soc_family'] != 'stm32h743' or result['backend'] != 'baremetal' or
            result['clock_profile'] != 'hsi64-fixed-cache-off' or
            len(controllers) != 1 or controllers[0]['id'] != 'bus4' or
            controllers[0]['kind'] != 'spi' or controllers[0]['mode'] != 'dma' or
            controllers[0]['controller'] != 'SPI4' or
            len(result['devices']) != 1 or
            result['devices'][0]['controller'] != 'bus4' or
            result['devices'][0].get('mode', 0) != 0):
        fail('power supports only baremetal H743 fixed64 cache-off SPI4 finite DMA')
    return RuntimePowerIR('fixed-run', ('bus4',))


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
    """Use the same emitted I/O helpers as cold startup and global cleanup."""
    try:
        from .providers.stm32h743 import emission
    except ImportError:
        from providers.stm32h743 import emission
    source = [
        'nx_result_t nx_platform_runtime_pause(void) {',
        f'    if ({context_guard}) {{ return NX_ERROR_CONTEXT; }}',
        '    if (!s_nx_platform_started) { return NX_ERROR_STATE; }',
        '    return nx_platform_stop_io(NULL, false);', '}',
        'nx_platform_start_result_t nx_platform_runtime_resume(void) {',
        '    nx_platform_start_result_t result = { NX_SUCCESS, NX_SUCCESS, 0u };',
        f'    if ({context_guard}) {{ result.primary = NX_ERROR_CONTEXT; return result; }}',
        '    if (!s_nx_platform_started || s_nx_runtime_lease.owner != NULL) { result.primary = NX_ERROR_STATE; return result; }',
        '    result.primary = nx_runtime_reopen();',
        '    if (result.primary != NX_SUCCESS) { return result; }',
        '    result.primary = nx_runtime_acquire();',
        '    if (result.primary != NX_SUCCESS) { return result; }']
    source.extend(emission.start_resources(result, cleanup='nx_platform_stop_io(NULL, true)'))
    for index, expression in enumerate(initialize):
        source += [f'    s_nx_platform_initialized = {index + 1}u;',
                   f'    result.primary = {expression};',
                   '    if (result.primary != NX_SUCCESS) {',
                   '        result.cleanup = nx_platform_stop_io(NULL, true);',
                   '        result.remaining_effects = result.cleanup == NX_SUCCESS ? 0u : 1u;',
                   '        return result;', '    }']
    return source + ['    return result;', '}']

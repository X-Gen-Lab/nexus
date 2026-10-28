"""Exact H743 SPI4 fixed-run lifecycle admission and construction."""
from tools.configure.providers.common import fail
from tools.configure.ir import RuntimePowerIR


def resolve_runtime_power(value, result):
    """Admit only the explicit single SPI4 finite-DMA software reference."""
    if (not isinstance(value, dict) or set(value) != {'mode', 'controllers'} or
            value['mode'] != 'fixed-run' or
            value['controllers'] != ['bus4']):
        fail('power requires fixed-run and exactly controllers=["bus4"]')
    controllers = result.controllers
    if (result.soc_family != 'stm32h743' or result.backend != 'baremetal' or
            result.clock_profile != 'hsi64-fixed-cache-off' or
            len(controllers) != 1 or controllers[0].id != 'bus4' or
            controllers[0].kind != 'spi' or controllers[0].mode != 'dma' or
            controllers[0].controller != 'SPI4' or
            len(result.devices) != 1 or
            result.devices[0].controller != 'bus4' or
            result.devices[0].options.mode != 0):
        fail('power supports only baremetal H743 fixed64 cache-off SPI4 finite DMA')
    return RuntimePowerIR('fixed-run', ('bus4',))



def runtime_functions(result, context_guard, initialize):
    """Use the same emitted I/O helpers as cold startup and global cleanup."""
    from . import emission
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

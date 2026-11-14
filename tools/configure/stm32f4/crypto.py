"""Static crypto engine connections; C owns gates, vectors and secret cleanup."""
from tools.configure.calls import Address, DirectCall, InitializeSequence, Symbol
from tools.configure.memory import dma_regions
from tools.configure.providers.common import Construction, kernel_policy_constant


def constructor(item, result):
    name, kind = item.id.replace('-', '_'), item.kind
    irq = Symbol('CRYP_IRQn' if kind == 'cipher' else 'HASH_RNG_IRQn')
    state = Address('s_nx_port_' + name)
    config = 's_nx_crypto_config_' + name
    lease = 's_nx_crypto_irq_' + name
    policy = result.irq
    definitions = []
    dma_config = 'NULL'
    if item.mode == 'dma-blocks':
        regions, count = dma_regions(name, result, include_flash=True,
                                      engine='DMA2')
        dma_state = 's_nx_crypto_dma_' + name
        dma_config = 's_nx_crypto_dma_config_' + name
        definitions += regions + [
            f'static nx_stm32f4_{kind}_dma_state_t {dma_state};',
            f'static const nx_stm32f4_{kind}_dma_config_t {dma_config} = {{',
            f'    &{dma_state}, s_nx_dma_regions_{name}, {count}u', '};']
        dma_config = '&' + dma_config
    definitions += [
        f'static nx_stm32f4_crypto_irq_t {lease};',
        f'static const nx_stm32f4_{kind}_config_t {config} = {{',
        f'    .irq_policy = {{ {kernel_policy_constant(result)}, '
        f'{policy.external_count}u, {policy.priority_bits}u, '
        f'{policy.syscall_priority or 0}u }},',
        f'    .dma = {dma_config}', '};',
        f'void {irq.name.removesuffix("_IRQn")}_IRQHandler(void);',
        r'/** \brief Dispatch the single statically selected crypto publisher. */',
        f'void {irq.name.removesuffix("_IRQn")}_IRQHandler(void) {{ '
        f'nx_stm32f4_{kind}_irq(&{state.name}); }}',
    ]
    vector_args = (Address(lease), irq, item.options.irq.priority)
    engine_args = (state, Address(config))
    sequence = InitializeSequence('nx_crypto_start_' + name, (
        DirectCall('nx_stm32f4_crypto_irq_acquire', vector_args),
        DirectCall(f'nx_stm32f4_{kind}_start', engine_args)))
    return Construction(
        state_type=f'nx_stm32f4_{kind}_state_t', definitions=definitions,
        admission=(DirectCall('nx_stm32f4_crypto_irq_admit', vector_args),
                   DirectCall(f'nx_stm32f4_{kind}_admit', engine_args)),
        initialize_sequences=(sequence,), initialize=DirectCall(sequence.function),
        stop=(DirectCall(f'nx_{kind}_port_stop', (Address('s_nx_face_' + name),)),
              DirectCall('nx_stm32f4_crypto_irq_release', (Address(lease), irq))),
        retain_on_failed_init=True)

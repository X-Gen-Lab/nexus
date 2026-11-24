"""Static coupled-domain references and fixed C lifecycle connections.

The validated immutable group selects every object and call. Concrete C owns
admission, clocks, interrupts, waiting and rollback; this module adds no behavior.
"""
from dataclasses import replace

from tools.configure.calls import Address, DirectCall, InitializeSequence, Symbol
from tools.configure.static_ir import Array, Declaration, IrqConnection, Record


def _name(domain):
    return domain.members[0].replace("-", "_")


def states(domains):
    """Define exactly one zero-initialized shared state before member configs."""
    return tuple(Declaration(item.state_type, "s_nx_domain_" + _name(item))
                 for item in domains)


def configurations(domains):
    """Connect existing member storage after all member definitions exist."""
    result = []
    for domain in domains:
        name = _name(domain)
        members = "s_nx_domain_members_" + name
        configs = "s_nx_domain_configurations_" + name
        result.extend((
            Declaration(domain.member_state_type, members,
                Array(tuple(Address("s_nx_port_" + item.replace("-", "_"))
                            for item in domain.members)),
                count=len(domain.members), pointer=True, const=True),
            Declaration(domain.member_config_type, configs,
                Array(tuple(Address("s_nx_config_" + item.replace("-", "_"))
                            for item in domain.members)),
                count=len(domain.members), pointer=True, const=True,
                pointee_const=True),
            Declaration(domain.config_type, "s_nx_domain_config_" + name,
                Record((("members", Symbol(members)),
                        ("configurations", Symbol(configs)),
                        ("count", len(domain.members)),
                        ("time_shared", domain.timing is not None),
                        ("start_tolerance_us", domain.timing.start_tolerance_us
                         if domain.timing is not None else 0))), const=True)))
    return tuple(result)


def construction(value, item, domains):
    """Prepare a domain with its first member; release it after that member."""
    domain = next((entry for entry in domains if item.id in entry.members), None)
    if domain is None:
        return value
    irqs = tuple(entry for entry in value.irq_connections
                 if entry.name != domain.handler)
    if item.id != domain.members[0]:
        return replace(value, irq_connections=irqs)
    name = _name(domain)
    state = Address("s_nx_domain_" + name)
    config = Address("s_nx_domain_config_" + name)
    sequence = InitializeSequence("nx_prepare_domain_" + name,
        (DirectCall(domain.prepare, (state, config)), value.initialize))
    return replace(value, irq_connections=irqs,
        admission=(DirectCall(domain.admit, (state, config)), *value.admission),
        initialize=DirectCall(sequence.function),
        initialize_sequences=(*value.initialize_sequences, sequence),
        stop=(*value.stop, DirectCall(domain.close, (state,))))


def interrupts(domains):
    """Each common interrupt has one fixed domain dispatcher."""
    return tuple(IrqConnection(item.handler,
        (DirectCall(item.adc_irq, (Address("s_nx_domain_" + _name(item)),)),),
        Symbol("nx_arch_dsb")) for item in domains)

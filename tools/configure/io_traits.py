"""Selected readonly I/O traits; no device discovery or hardware qualification.

The provider's maintained mode gate is authoritative. These records document
the fixed public formats implemented today, rather than theoretical SoC modes.
The factory's existing capability switch owns emission; disabled means no new
firmware objects. Clock ceilings are authored limits, never measured rates.
"""
from collections.abc import Mapping
from dataclasses import dataclass, field
import re
from types import MappingProxyType

try:
    from . import selected_contracts
    from .providers.common import fail, integer
except ImportError:
    import selected_contracts
    from providers.common import fail, integer


_KINDS = selected_contracts.FORMAT_KINDS
_FIELDS = selected_contracts.FORMAT_FIELDS


@dataclass(frozen=True)
class SelectedTraits:
    """Immutable projection; buffer loans and provider checks still apply."""
    kind: str
    fields: Mapping[str, int]
    abort_scope: str
    continuity: str
    timestamp_source: str
    dma_policy: str
    cache_line_bytes: int

    def __post_init__(self):
        object.__setattr__(self, "fields", MappingProxyType(dict(self.fields)))


def describe(item):
    """Project one validated controller without broadening provider support.

    No timestamp claim is made where the public mode exposes no timestamp.
    Model records never establish IRQ time, physical continuity or DMA reach.
    Cache line size describes the selected DMA provider's cache policy when
    applicable, not proof that cache is enabled in the selected clock profile.
    """
    if item.kind not in _KINDS:
        return None
    selected = selected_contracts.describe(item)
    return SelectedTraits(item.kind, selected.fields, selected.abort_scope,
                          selected.continuity, selected.timestamp_source,
                          selected.dma_policy, selected.cache_line_bytes)


def emit_controllers(controllers):
    """Emit selected-class const records into the existing factory bundle.

    IDs and includes are already declared by factory.emit. Endpoints keep the
    controller's traits; no duplicate per-endpoint objects are generated here.
    Unknown is distinct from not-applicable; neither means unlimited support.
    """
    selected = tuple((item, describe(item)) for item in controllers
                     if item.kind in _KINDS)
    if not selected:
        return [], []
    header = [
        '#include "nexus/io/observation.h"',
        '/** \\brief Abort effects never release borrowed storage. */',
        'typedef enum {',
        '    NX_IO_ABORT_UNKNOWN = 0,',
        '    NX_IO_ABORT_REQUEST,',
        '    NX_IO_ABORT_CONTROLLER_PENDING,',
        '    NX_IO_ABORT_NOT_APPLICABLE',
        '} nx_io_abort_scope_t;',
        '/** \\brief Receive mechanism without throughput qualification. */',
        'typedef enum {',
        '    NX_IO_CONTINUITY_UNKNOWN = 0,',
        '    NX_IO_CONTINUITY_NOT_APPLICABLE,',
        '    NX_IO_CONTINUITY_IRQ_BUFFERED,',
        '    NX_IO_CONTINUITY_REARM_GAP,',
        '    NX_IO_CONTINUITY_DOUBLE_BUFFER,',
        '    NX_IO_CONTINUITY_MODEL',
        '} nx_io_continuity_t;',
        '/** \\brief Selected DMA provider owns all memory checks. */',
        'typedef enum {',
        '    NX_IO_DMA_POLICY_NONE = 0,',
        '    NX_IO_DMA_POLICY_PROVIDER_DOMAIN,',
        '    NX_IO_DMA_POLICY_DOMAIN_CACHE_LINE',
        '} nx_io_dma_policy_t;',
    ]
    source = []
    groups = {}
    for item, record in selected:
        namespace = ("uart_word" if item.mode == "irq-words" else
                     item.interface_kind or item.kind)
        groups.setdefault((item.kind, namespace), []).append((item, record))
    for (kind, namespace), instances in sorted(groups.items()):
        typename = f"nx_factory_{namespace}_traits_t"
        header += [
            '/**',
            ' * \\brief Readonly selected format and execution restrictions.',
            ' * \\note No clock measurement, physical HIL or unbounded claim.',
            ' * Parity zero means none. Cache line applies to provider policy;',
            ' * it does not assert that cache is enabled. Unknown timestamp',
            ' * means no usable source described, never event time zero.',
            ' */',
            'typedef struct {',
            *[f"    uint32_t {name};" for name in _FIELDS[kind]],
            '    nx_io_abort_scope_t abort_scope;',
            '    nx_io_continuity_t continuity;',
            '    nx_io_timestamp_source_t timestamp_source;',
            '    nx_io_dma_policy_t dma_policy;',
            '    uint32_t cache_line_bytes;',
            f'}} {typename};',
            '/** \\brief Pure lookup; NULL means invalid ID. */',
            f'const {typename}* nx_factory_{namespace}_traits(',
            f'    nx_{namespace}_id_t id);',
        ]
        for item, record in instances:
            name = item.id.replace("-", "_")
            values = [f"{record.fields[field]}U" for field in _FIELDS[kind]]
            values += ["NX_IO_ABORT_" + record.abort_scope.upper(),
                       "NX_IO_CONTINUITY_" + record.continuity.upper(),
                       "NX_IO_TIMESTAMP_" + record.timestamp_source.upper(),
                       "NX_IO_DMA_POLICY_" + record.dma_policy.upper(),
                       f"{record.cache_line_bytes}U"]
            declaration = f"s_nx_traits_{kind}_{name} = {{"
            source += [f"static const {typename} {declaration}",
                       *[f"    {value}," for value in values], '};']
        source += ['/** \\brief Pure selected-mode traits lookup. */',
                   f'const {typename}* nx_factory_{namespace}_traits(',
                   f'    nx_{namespace}_id_t id) {{', '    switch (id) {']
        for item, _ in instances:
            name = item.id.replace("-", "_")
            source += [f'        case NX_{namespace.upper()}_ID_{name.upper()}:',
                       f'            return &s_nx_traits_{kind}_{name};']
        source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


@dataclass(frozen=True)
class RevisionPolicy:
    """Explicit software review admission, never physical qualification."""
    part: str
    device_id: int
    document_revision: str
    document_sha256: str
    reviewed: tuple[int, ...]
    blocked: tuple[int, ...]

    def __post_init__(self):
        if (not isinstance(self.part, str) or
                re.fullmatch("[A-Za-z][A-Za-z0-9_-]*", self.part) is None):
            fail("Revision admission requires an exact part")
        integer(self.device_id, context="Revision device ID")
        if (not isinstance(self.document_revision, str) or
                not self.document_revision.strip() or
                not isinstance(self.document_sha256, str) or
                re.fullmatch("[0-9a-f]{64}", self.document_sha256) is None):
            fail("Revision admission requires exact reviewed document identity")
        for values in (self.reviewed, self.blocked):
            if not isinstance(values, tuple):
                fail("Revision admission requires unique immutable IDs")
            for value in values:
                integer(value, context="Silicon revision ID")
            if len(set(values)) != len(values):
                fail("Revision admission requires unique immutable IDs")
        if set(self.reviewed).intersection(self.blocked):
            fail("Silicon revision cannot be both reviewed and blocked")


@dataclass(frozen=True)
class RevisionAdmission:
    """No admission result establishes HIL, electrical or timing evidence."""
    status: str
    allowed: bool
    hardware_qualified: bool = field(default=False, init=False)


def admit_revision(policy, part, device_id, revision_id):
    """Compare explicit observed identity; missing evidence is not repaired."""
    if not isinstance(policy, RevisionPolicy):
        fail("Revision admission requires an immutable reviewed policy")
    integer(device_id, context="Observed device ID")
    if part != policy.part or device_id != policy.device_id:
        fail("Observed part/device differs from reviewed revision policy")
    if revision_id is None:
        return RevisionAdmission("not_observed", False)
    integer(revision_id, context="Observed revision ID")
    if revision_id in policy.blocked:
        return RevisionAdmission("blocked", False)
    if revision_id in policy.reviewed:
        return RevisionAdmission("software_reviewed", True)
    return RevisionAdmission("unreviewed", False)

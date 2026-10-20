"""Immutable selected DMA lanes and bounded static descriptor placement.

Facts describe software-selected routes, not every DMA capability in silicon.
Descriptor validation establishes a configuration contract only; a runtime
provider must still borrow storage, stop requests, drain hardware and publish
SETTLED before reclaiming descriptors or payloads.
"""

from dataclasses import asdict, dataclass
import re

try:
    from .resource_graph import fail, identifier, json_value, number, record
except ImportError:
    from resource_graph import fail, identifier, json_value, number, record


LIMIT = 1 << 32


@dataclass(frozen=True)
class DmaLaneIR:
    owner: str
    controller: str
    engine: str
    lane: int
    request: int
    physical_key: str


@dataclass(frozen=True)
class DmaChainIR:
    owner: str
    engine: str
    lane: int
    node_capacity: int
    node_bytes: int
    alignment: int
    window_bytes: int
    region: str
    block_bytes: int
    maximum_bytes: int
    finite: bool


@dataclass(frozen=True)
class DmaTopologyIR:
    lanes: tuple[DmaLaneIR, ...]
    chains: tuple[DmaChainIR, ...] = ()

    @classmethod
    def from_validated(cls, values):
        """Freeze tuples checked against the selected provider topology."""
        return cls(tuple(DmaLaneIR(**item) for item in values["lanes"]),
                   tuple(DmaChainIR(**item) for item in values.get("chains", ())))

    def to_dict(self):
        return {"lanes": [asdict(item) for item in self.lanes],
                "chains": [asdict(item) for item in self.chains]}


@dataclass(frozen=True)
class DmaRegionIR:
    id: str
    origin: int
    size: int
    engines: tuple[str, ...]
    security: str
    alignment: int
    cache_line_bytes: int


@dataclass(frozen=True)
class DmaDescriptorIR:
    id: str
    address: int
    size: int
    payload_address: int
    payload_bytes: int
    engine: str
    security: str
    next: str | None
    region: str
    payload_region: str


@dataclass(frozen=True)
class DmaDescriptorPlanIR:
    descriptors: tuple[DmaDescriptorIR, ...]
    regions: tuple[DmaRegionIR, ...]
    cyclic: bool
    stop_owner: str | None

    def to_dict(self):
        return {"descriptors": [json_value(asdict(item))
                                for item in self.descriptors],
                "regions": [json_value(asdict(item)) for item in self.regions],
                "cyclic": self.cyclic, "stop_owner": self.stop_owner}


def resolve_selected(selected, provider):
    """Project validated provider tuples without reinterpreting aliases."""
    lanes, chains, occupied = [], [], set()
    for item in selected:
        chain_hook = getattr(provider, "dma_chain_plan", None)
        chain = chain_hook(item) if callable(chain_hook) else None
        if chain is not None:
            chains.append(DmaChainIR(**chain))
        for path in item.get("dma", ()):
            identifier(path, "DMA route")
            engine, lane, request = provider.validate_dma_resource(
                "dma:" + path)
            number(lane, 0, 65535, "DMA physical lane")
            number(request, 0, 65535, "DMA request selector")
            if type(engine) is str:
                # Named engines need an explicit family physical-identity hook;
                # channel-based GPDMA is never reinterpreted as a DMA stream.
                physical_key = getattr(provider, "dma_physical_key", None)
                if (re.fullmatch(r"[A-Z][A-Z0-9_]*", engine) is None or
                        not callable(physical_key)):
                    fail("Named DMA engine requires a physical identity hook")
                physical = identifier(physical_key("dma:" + path),
                                      "DMA physical identity")
                if not physical.startswith(f"dma:{engine}:"):
                    fail("DMA physical identity must name its actual engine")
                engine_name = engine
            else:
                number(engine, 0, 255, "DMA engine")
                engine_name = f"DMA{engine}"
                physical = f"dma:{engine_name}:stream{lane}"
            if physical in occupied:
                fail(f"DMA physical lane conflict: {physical}")
            occupied.add(physical)
            lanes.append(DmaLaneIR(identifier(item["id"], "DMA owner"),
                                   identifier(item["controller"],
                                              "DMA peripheral"),
                                   engine_name, lane, request, physical))
    return DmaTopologyIR(tuple(lanes), tuple(chains))


def _power(value, context, zero=False):
    number(value, 0 if zero else 1, LIMIT // 2, context)
    if value and value & (value - 1):
        fail(f"{context} must be a power of two")
    return value


def _span(address, size, context):
    number(address, 0, LIMIT - 1, context + " address")
    number(size, 1, LIMIT, context + " size")
    if size > LIMIT - address:
        fail(f"{context} interval overflows the address space")
    return address, address + size


def _overlap(left, right):
    return left[0] < right[1] and right[0] < left[1]


def _placement(address, size, engine, security, regions, context):
    start, end = _span(address, size, context)
    matches = [region for region in regions
               if region.origin <= start and
               end <= region.origin + region.size and
               engine in region.engines and security == region.security]
    if len(matches) != 1:
        fail(f"{context} has no unambiguous accessible DMA region")
    region = matches[0]
    alignment = max(region.alignment, region.cache_line_bytes)
    if start % alignment or size % alignment:
        fail(f"{context} violates DMA/cache-line ownership alignment")
    return region.id


def resolve_descriptors(descriptors, regions, *, cyclic=False, stop_owner=None):
    """Validate explicitly placed descriptor and payload intervals.

Cacheable storage owns complete cache lines. Regions may not overlap, including
aliases: callers must first resolve physical backing. Shared payload intervals
are rejected until a mode defines explicit access ordering and ownership.
"""
    if type(cyclic) is not bool:
        fail("DMA cyclic mode must be boolean")
    if cyclic:
        identifier(stop_owner, "DMA cyclic stop owner")
    elif stop_owner is not None:
        fail("Finite DMA descriptors do not accept a cyclic stop owner")
    domains = []
    for value in regions:
        record(value, {"id", "origin", "size", "engines", "security",
                       "alignment"},
               {"cache_line_bytes"}, "DMA region")
        name = identifier(value["id"], "DMA region")
        span = _span(value["origin"], value["size"], "DMA region")
        engines = value["engines"]
        if (not isinstance(engines, (list, tuple)) or not engines or
                any(type(engine) is not str for engine in engines) or
                len(set(engines)) != len(engines)):
            fail("DMA region requires unique accessible engines")
        engines = tuple(identifier(engine, "DMA accessible engine")
                        for engine in engines)
        security = value["security"]
        if type(security) is not str or security not in {
                "single", "secure", "nonsecure"}:
            fail("Invalid DMA security domain")
        alignment = _power(value["alignment"], "DMA alignment")
        cache = _power(value.get("cache_line_bytes", 0),
                       "DMA cache line", zero=True)
        if any(item.id == name or
               _overlap(span, (item.origin, item.origin + item.size))
               for item in domains):
            fail("Overlapping or duplicated DMA regions")
        domains.append(DmaRegionIR(name, span[0], value["size"], engines,
                                   security, alignment, cache))
    result, spans = [], []
    for value in descriptors:
        record(value, {"id", "address", "size", "payload_address",
                       "payload_bytes", "engine", "security", "next"},
               set(), "DMA descriptor")
        name = identifier(value["id"], "DMA descriptor")
        if any(item.id == name for item in result):
            fail("Duplicate DMA descriptor")
        engine = identifier(value["engine"], "DMA descriptor engine")
        security = value["security"]
        desc = _span(value["address"], value["size"], "DMA descriptor")
        payload = _span(value["payload_address"], value["payload_bytes"],
                        "DMA payload")
        if (_overlap(desc, payload) or any(_overlap(current, previous)
                for current in (desc, payload) for previous in spans)):
            fail("DMA descriptor/payload storage overlap")
        desc_region = _placement(value["address"], value["size"], engine,
                                 security, domains, "DMA descriptor")
        payload_region = _placement(value["payload_address"],
                                    value["payload_bytes"], engine, security,
                                    domains, "DMA payload")
        next_name = value["next"]
        if next_name is not None:
            identifier(next_name, "DMA next descriptor")
        result.append(DmaDescriptorIR(name, value["address"], value["size"],
                                       value["payload_address"],
                                       value["payload_bytes"], engine, security,
                                       next_name, desc_region, payload_region))
        spans.extend((desc, payload))
    names = {item.id: item for item in result}
    if any(item.next is not None and item.next not in names for item in result):
        fail("DMA descriptor references an unknown next node")
    for item in result:
        if item.next is not None and (
                names[item.next].engine != item.engine or
                names[item.next].security != item.security):
            fail("DMA descriptor chain crosses engine or security ownership")
    has_cycle = False
    for first in names:
        visited, name = set(), first
        while name is not None:
            if name in visited:
                has_cycle = True
                break
            visited.add(name)
            name = names[name].next
    if has_cycle != cyclic:
        fail("DMA descriptor cycle requires explicit stoppable cyclic mode")
    return DmaDescriptorPlanIR(tuple(result), tuple(domains),
                               cyclic, stop_owner)

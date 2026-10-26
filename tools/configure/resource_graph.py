"""Immutable compile-time resource ownership; no runtime registry is emitted.

Legacy claims are exclusive. Sharing needs an exact reviewed resource key and
the complete attribute set supplied by hardware facts, never an authored flag
alone. Slash-delimited children belong to their canonical physical parent.
The explicit ``ip:<identity>`` namespace owns one personality across all of its
descendants; a controller alias supplies that reviewed physical identity.
"""

from collections.abc import Mapping
from dataclasses import asdict, dataclass
import re

from tools.configure.providers.common import fail


def identifier(value, context):
    """Validate stable graph identities without filesystem path semantics."""
    if (type(value) is not str or not value or len(value) > 256 or
            re.fullmatch(r"[A-Za-z0-9_:@./-]+", value) is None or
            any(part in {"", ".", ".."} for part in value.split("/"))):
        fail(f"Invalid {context}: {value!r}")
    return value


def record(value, required, optional, context):
    if (not isinstance(value, Mapping) or
            not set(required).issubset(value) or
            set(value) - set(required) - set(optional)):
        fail(f"Invalid {context} fields")
    return value


def number(value, low, high, context):
    if type(value) is not int or not low <= value <= high:
        fail(f"Invalid {context}: {value!r}")
    return value


def json_value(value):
    """Project frozen tuples into fresh JSON containers."""
    if isinstance(value, Mapping):
        return {key: json_value(item) for key, item in value.items()}
    if isinstance(value, (tuple, list)):
        return [json_value(item) for item in value]
    return value


def _text(value, context):
    if type(value) is not str or not value or "\0" in value:
        fail(f"Invalid {context}")
    return value


@dataclass(frozen=True)
class ResourceClaimIR(Mapping):
    key: str
    owner: str
    mode: str
    source: str
    policy: str
    attributes: tuple[tuple[str, int | bool | str], ...]
    personality: str | None

    _FIELDS = ("key", "owner", "mode", "source", "policy", "attributes",
               "personality")

    def __getitem__(self, key):
        if key not in self._FIELDS:
            raise KeyError(key)
        return getattr(self, key)

    def __iter__(self):
        return iter(self._FIELDS)

    def __len__(self):
        return len(self._FIELDS)


@dataclass(frozen=True)
class ResourcePlanIR:
    claims: tuple[ResourceClaimIR, ...]

    @classmethod
    def from_validated(cls, values):
        """Freeze canonical resolver output without selecting facts."""
        claims = []
        for item in values["claims"]:
            attributes = tuple(tuple(pair) for pair in item["attributes"])
            claims.append(ResourceClaimIR(**{**item, "attributes": attributes}))
        return cls(tuple(claims))

    def to_dict(self):
        return {"claims": [json_value(asdict(item)) for item in self.claims]}


def _canonical(key, aliases):
    visited = set()
    while True:
        candidates = sorted(aliases, key=len, reverse=True)
        matched = next((alias for alias in candidates
                        if key == alias or key.startswith(alias + "/")), None)
        if matched is None:
            return key
        if matched in visited:
            fail("Resource alias cycle or expanding alias")
        visited.add(matched)
        target = aliases[matched] + key[len(matched):]
        if target == key or target.startswith(key + "/"):
            fail("Resource alias cycle or expanding alias")
        key = target


def _attributes(values):
    if not isinstance(values, Mapping):
        fail("Resource attributes must be a mapping")
    result = []
    for key, value in values.items():
        identifier(key, "resource attribute")
        if type(value) not in {int, bool, str}:
            fail("Resource attributes must be scalar")
        if type(value) is int:
            number(value, -(1 << 63), (1 << 64) - 1, "resource attribute")
        if type(value) is str:
            _text(value, "resource attribute")
        result.append((key, value))
    return tuple(sorted(result))


def _same_attributes(left, right):
    return (len(left) == len(right) and
            all(a == b and type(x) is type(y) and x == y
                for (a, x), (b, y) in zip(left, right)))


def conflict(key, previous, current, reason="Resource conflict"):
    """Preserve both canonical owners for human and machine diagnostics."""
    fail(f"{reason} {key}: {previous.owner} (source {previous.source}) "
         f"versus {current.owner} (source {current.source})",
         code="resource_conflict", resource=key,
         claims=[json_value(asdict(previous)), json_value(asdict(current))])


def resolve(records, *, aliases=None, shared=None):
    """Resolve validated fact aliases and explicit compatible subresources.

An exclusive parent blocks every child owned by a different owner. Distinct
children may have distinct owners; their common base must be separately claimed
with compatible attributes. Existing controller reservations stay exclusive.
"""
    aliases = {} if aliases is None else aliases
    shared = {} if shared is None else shared
    if not isinstance(aliases, Mapping) or not isinstance(shared, Mapping):
        fail("Resource alias and sharing facts must be mappings")
    for alias, target in aliases.items():
        identifier(alias, "resource alias")
        identifier(target, "resource alias target")
    for alias in aliases:
        _canonical(alias, aliases)
    sharing = {}
    for key, fields in shared.items():
        canonical = _canonical(identifier(key, "shared resource"), aliases)
        if (not isinstance(fields, (tuple, list)) or not fields or
                any(type(field) is not str for field in fields) or
                len(set(fields)) != len(fields) or canonical in sharing):
            fail("Invalid or duplicated resource sharing facts")
        sharing[canonical] = tuple(sorted(identifier(field, "shared attribute")
                                          for field in fields))
    result = []
    personalities = {}
    for value in records:
        record(value, {"key", "owner", "mode", "source"},
               {"policy", "attributes", "personality"}, "resource claim")
        key = _canonical(identifier(value["key"], "resource key"), aliases)
        policy = value.get("policy", "exclusive")
        if type(policy) is not str or policy not in {"exclusive", "compatible"}:
            fail("Unknown resource claim policy")
        attrs = _attributes(value.get("attributes", {}))
        fields = tuple(name for name, _ in attrs)
        if policy == "compatible" and (
                key not in sharing or fields != sharing[key]):
            fail("Compatible resource requires exact reviewed sharing facts")
        personality = value.get("personality")
        item = ResourceClaimIR(key, _text(value["owner"], "resource owner"),
                               _text(value["mode"], "resource mode"),
                               _text(value["source"], "resource source"),
                               policy, attrs, personality)
        if personality is not None:
            identifier(personality, "IP personality")
            physical_ip = key.partition("/")[0]
            if (not physical_ip.startswith("ip:") or
                    len(physical_ip) == len("ip:")):
                fail("IP personality requires an explicit physical IP identity")
            previous_personality = personalities.get(physical_ip)
            if (previous_personality is not None and
                    previous_personality.personality != personality):
                conflict(physical_ip, previous_personality, item,
                         "Resource personality conflict")
            personalities[physical_ip] = item
        duplicate = False
        for previous in result:
            related = (key == previous.key or
                       key.startswith(previous.key + "/") or
                       previous.key.startswith(key + "/"))
            if not related:
                continue
            if (personality is not None and previous.personality is not None and
                    personality != previous.personality):
                conflict(key, previous, item, "Resource personality conflict")
            if key == previous.key:
                if (item == previous and
                        _same_attributes(attrs, previous.attributes)):
                    duplicate = True
                    break
                if (policy == previous.policy == "compatible" and
                        _same_attributes(attrs, previous.attributes)):
                    continue
                conflict(key, previous, item)
            if item.owner != previous.owner:
                conflict(key, previous, item, "Resource parent/child conflict")
        if not duplicate:
            result.append(item)
    return ResourcePlanIR(tuple(result))


def normalize_resource(value):
    timer = re.fullmatch(r"(?:timer:|controller:)?(TIM[0-9]+|TIMER[0-9]+)", value)
    if timer is not None:
        return "controller:" + timer[1]
    if value.startswith("dma:"):
        channel = re.fullmatch(r"dma:(GPDMA1):channel(0|[1-9][0-9]*)"
                               r"(?::request(0|[1-9][0-9]*))?", value)
        if channel is not None:
            return f"dma:{channel[1]}:channel{channel[2]}"
        match = re.fullmatch(r"dma:([A-Z][A-Z0-9_]*):stream([0-9]+)"
                             r"(?::channel([0-9]+))?", value)
        if match is None:
            fail(f"Invalid physical DMA resource: {value}")
        return f"dma:{match[1]}:stream{int(match[2])}"
    if value.startswith("irq:"):
        irq = value[4:]
        for suffix in ("_IRQHandler", "_IRQn", "_Handler"):
            if irq.endswith(suffix):
                return "irq:" + irq[:-len(suffix)]
    return value

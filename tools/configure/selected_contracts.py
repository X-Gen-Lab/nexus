"""One selected I/O contract projection, without runtime device discovery.

SoC facts, exact routes and the actual provider mode gate establish support.
Validated typed options establish the selected public face. C limit expressions
remain expressions; unknown bounds never become unlimited hardware capacity.
These compiler records add no firmware objects unless an existing consumer emits
its explicitly selected readonly metadata.
"""
from collections.abc import Mapping
from dataclasses import dataclass, field
from types import MappingProxyType

try:
    from .providers import maintained
    from .providers.common import fail, mode_traits
except ImportError:
    from providers import maintained
    from providers.common import fail, mode_traits


ENDPOINT_DRIVERS = MappingProxyType({
    "spi-transaction": ("spi", "spi_transaction_device"),
    "i2c-long-endpoint": ("i2c", "i2c_long_device"),
    "spi-dma-transaction": ("spi", "spi_dma_transaction_device"),
})
FORMAT_KINDS = ("uart", "spi", "i2c", "adc", "can", "eth", "eth_frames")
FORMAT_FIELDS = MappingProxyType({
    "uart": ("data_bits", "parity", "stop_half_bits", "requested_baud"),
    "spi": ("word_bits", "configured_max_hz"),
    "i2c": ("address_bits", "configured_max_hz"),
    "adc": ("storage_bits", "signed_samples", "scan_channels"),
    "can": ("classic_payload_bytes", "hardware_tx_mailboxes"),
    "eth": ("vlan", "scatter_gather"),
    "eth_frames": ("vlan", "scatter_gather", "clock_domain"),
})
# Adding a provider/mode does not inherit a public-format implementation review.
_FORMAT_PROVIDERS = frozenset((
    "native", "stm32f103", "stm32f407", "stm32g474", "stm32h743",
    "stm32u575", "gd32f303", "gd32f470"))


@dataclass(frozen=True)
class SupportContract:
    """Declared exact-part support; route presence is not selectability/HIL."""
    family: str
    part: str
    controller: str
    kind: str
    mode: str
    implementation: str
    reviewed_routes: tuple[str, ...]
    review_sources: tuple[str, ...]
    physical_qualified: bool = field(default=False, init=False)

    @property
    def reviewed_route_available(self):
        return bool(self.reviewed_routes)


@dataclass(frozen=True)
class SelectedContract:
    """Nominal public units and limits of one validated static identity.

    Limits are integers, existing C expressions, or None (not described).
    unit_bytes=None means no fixed byte width; typed units may be non-byte
    counts or per-request words, never a zero-sized payload. A
    descriptor, message and hardware segment retain distinct count bounds.
    """
    id: str
    provider: str
    controller: str
    kind: str
    mode: str
    public_class: str
    model: bool
    features: tuple[str, ...]
    unit: str = "not_applicable"
    unit_bytes: int | None = None
    buffer_alignment: int = 1
    max_transfer_units: int | str | None = None
    max_messages: int | str | None = None
    max_segment_units: int | str | None = None
    max_segments: int | str | None = None
    max_total_units: int | str | None = None
    word_bits: tuple[int, ...] = ()
    dma_engines: tuple[str, ...] = ()
    fields: Mapping = field(default_factory=dict)
    abort_scope: str = "not_applicable"
    continuity: str = "not_applicable"
    timestamp_source: str = "unknown"
    dma_policy: str = "none"
    cache_line_bytes: int = 0
    physical_qualified: bool = field(default=False, init=False)

    def __post_init__(self):
        object.__setattr__(self, "fields", MappingProxyType(dict(self.fields)))


@dataclass(frozen=True)
class SiliconPolicy:
    """Selected review expectation, never an observed silicon identity.

    This initial projection describes the existing clock review only. It does
    not establish a complete peripheral errata review or execute admission.
    """
    part: str
    provider: str
    profile: str
    reviewed_revision_ids: tuple[int, ...]
    review_scope: str
    unknown_revision: str = field(default="reject", init=False)
    physical_qualified: bool = field(default=False, init=False)


def controller_class(item):
    """Physical controller and nominal public face have separate identities."""
    return ("pin_state" if item.kind == "gpio" and item.mode == "named-states" else
            "uart_word" if item.mode == "irq-words" else
            item.interface_kind or item.kind)


def endpoint_class(item):
    """Never substitute an enhanced endpoint for a legacy public type."""
    if item.driver in ENDPOINT_DRIVERS:
        return ENDPOINT_DRIVERS[item.driver][1]
    return ("usb_endpoint" if item.kind == "usb_device"
            else item.kind + "_device")


def _features(shape, model):
    features = ["MODEL"] if model else []
    if shape.polling:
        features.append("POLLING")
    if not model:
        for enabled, name in ((shape.irq, "IRQ"), (shape.tx_dma, "TX_DMA"),
                              (shape.rx_dma, "RX_DMA"),
                              (shape.bus_master, "BUS_MASTER_DMA")):
            if enabled:
                features.append(name)
    if shape.block_stream:
        features.append("BLOCK_STREAM")
        if shape.dma and not shape.continuous:
            features.append("STREAM_GAPS")
    return tuple(features)


def _format(item, provider, shape):
    kind, options = item.kind, item.options
    fields = {}
    abort = "request" if shape.irq and kind != "adc" else "not_applicable"
    continuity, source = "not_applicable", "unknown"
    if kind in FORMAT_KINDS and item.provider not in _FORMAT_PROVIDERS:
        fail("I/O traits require an explicit provider format review")
    if kind == "uart":
        frame = options.frame
        fields = dict(data_bits=frame.data_bits if frame else 8,
                      parity=({"none": 0, "even": 1, "odd": 2}[frame.parity]
                              if frame else 0),
                      stop_half_bits=frame.stop_bits * 2 if frame else 2,
                      requested_baud=options.baud)
        continuity = ("model" if provider.MODEL else "double_buffer"
                      if shape.continuous else "rearm_gap"
                      if shape.rx_dma else "irq_buffered")
        if not provider.MODEL and options.rx_profile == "events":
            source = "irq_observation"
    elif kind == "spi":
        fields = dict(word_bits=0 if controller_class(item) ==
                      "spi_dma_transaction" else 8,
                      configured_max_hz=options.max_hz)
    elif kind == "i2c":
        fields = dict(address_bits=7, configured_max_hz=options.max_hz)
    elif kind == "adc":
        fields = dict(storage_bits=16, signed_samples=0,
                      scan_channels=len(options.channels))
        if shape.block_stream:
            continuity = "double_buffer" if shape.continuous else "rearm_gap"
            source = "irq_observation"
    elif kind == "can":
        if item.provider not in {"stm32f407", "gd32f470"}:
            fail("CAN traits require a reviewed physical mailbox topology")
        fields = dict(classic_payload_bytes=8, hardware_tx_mailboxes=3)
        source = "irq_observation"
    elif kind in {"eth", "eth_frames"}:
        enhanced = kind == "eth_frames"
        fields = dict(vlan=int(enhanced), scatter_gather=int(enhanced))
        if enhanced:
            fields["clock_domain"] = options.clock_domain
        abort = "controller_pending"
        source = "hardware_event" if enhanced else "acquire_observation"
    dma = not provider.MODEL and (shape.dma or shape.bus_master)
    line = provider.CPU_FEATURES["dcache_line_bytes"] if dma else 0
    policy = "none"
    if dma:
        policy = "domain_cache_line" if line else "provider_domain"
    if item.provider == "stm32h743" and (
            (kind == "spi" and item.mode == "dma") or
            (kind == "uart" and item.mode in
             ("rx-dma-continuous", "dma-duplex-continuous"))):
        line = options.dma_cache_line_bytes
        policy = "domain_cache_line" if line else "provider_domain"
    return dict(fields=fields, abort_scope=abort, continuity=continuity,
                timestamp_source=source, dma_policy=policy,
                cache_line_bytes=line)


def describe(item, endpoint=None, *, public_class=None):
    """Project a validated controller/child after its actual mode gate.

    A synthetic ControllerIR can exercise individual projections in tests; it
    does not establish a reviewed physical route or replace full resolution.
    """
    provider = maintained(item.provider)
    provider.mode_implementation(item.kind, item.mode)
    shape = mode_traits(item.kind, item.mode)
    selected = (endpoint_class(endpoint) if endpoint is not None else
                controller_class(item))
    if public_class is not None and public_class != selected:
        if not (public_class == "mdio" and item.kind in {"eth", "eth_frames"} and
                endpoint is None):
            fail("Capability class differs from the selected public face")
        selected = public_class
    if endpoint is not None and (endpoint.controller != item.id or
                                 endpoint.kind != item.kind):
        fail("Selected endpoint differs from its actual controller")
    kind = item.kind
    limits = {}
    if kind == "rtc":
        limits.update(unit="occurrences", max_transfer_units=1)
    elif kind == "lptim":
        limits.update(unit="ticks", max_transfer_units=65536)
    if kind in {"uart", "spi", "i2c", "can", "can_fd", "eth", "eth_frames",
                "usb_device"}:
        limits.update(unit="bytes", unit_bytes=1, word_bits=(8,))
    if kind in {"uart", "spi"} and shape.dma:
        limits["max_transfer_units"] = "UINT16_MAX"
    if item.provider == "stm32u575" and kind == "uart" and item.mode == "dma-chain-tx":
        limits["max_transfer_units"] = 196605
    if item.provider == "stm32h743" and kind == "uart" and item.mode == "rx-dma-continuous":
        # This public request is IRQ TX; the separate RX block loan has the
        # provider's 16-bit NDTR and optional complete-cache-line restrictions.
        limits["max_transfer_units"] = "UINT32_MAX"
    if (item.provider == "stm32h743" and kind == "uart" and
            item.mode == "dma-duplex-continuous" and
            item.options.dma_cache_line_bytes):
        limits.update(buffer_alignment=32, max_transfer_units="65504")
    if (item.provider == "stm32h743" and kind == "spi" and shape.dma and
            item.options.dma_cache_line_bytes):
        limits.update(buffer_alignment=32, max_transfer_units="65504")
    if kind == "spi" and not shape.dma and not provider.MODEL:
        limits["max_transfer_units"] = 256
    elif kind == "i2c" and not provider.MODEL:
        limits.update(max_transfer_units="NX_I2C_MAX_MESSAGE_BYTES",
                      max_messages="NX_I2C_MAX_MESSAGES")
    elif kind in {"adc", "pwm_adc"}:
        count = len(item.options.channels)
        bound = ("UINT16_MAX" if kind == "pwm_adc" else
                 f"UINT16_MAX - UINT16_MAX % {count}U" if shape.dma else count)
        limits.update(unit="samples", unit_bytes=2, buffer_alignment=2,
                      max_transfer_units=bound)
    elif kind == "adc_group":
        limits.update(unit="frames", unit_bytes=item.options.scan_stride,
                      buffer_alignment=2,
                      max_transfer_units=item.options.max_frames,
                      word_bits=(12,))
    elif kind in {"can", "can_fd"}:
        limits.update(max_transfer_units=64 if kind == "can_fd" else 8,
                      max_messages=1)
    elif kind == "eth":
        limits.update(max_transfer_units="NX_ETH_FRAME_MAX",
                      buffer_alignment=4, max_messages=1)
    elif kind == "eth_frames":
        limits.update(max_transfer_units="NX_ETH_FRAME_MAX + 4U",
                      buffer_alignment=4, max_segments=item.options.tx_count,
                      max_messages=1)
    elif kind == "usb_device":
        limits.update(max_transfer_units=endpoint.options.max_packet
                      if endpoint is not None else 64, max_messages=1)
    elif kind == "flash":
        unit = (1 if provider.MODEL else
                getattr(provider, "FLASH_PROGRAM_UNIT", None))
        alignment = (1 if provider.MODEL else
                     getattr(provider, "FLASH_INPUT_ALIGNMENT", None))
        if any(type(value) is not int or not 1 <= value <= 65535 or
               value & (value - 1) for value in (unit, alignment)):
            fail("Flash provider requires exact power-of-two units/alignment")
        limits.update(unit="program_units", unit_bytes=unit,
                      buffer_alignment=alignment)
    formatted = _format(item, provider, shape)
    if endpoint is not None and kind == "spi":
        formatted["fields"] = {
            **formatted["fields"],
            "configured_max_hz": endpoint.options.max_hz}
    if selected == "uart_word":
        limits.update(unit="words", unit_bytes=2, buffer_alignment=2,
                      max_transfer_units=None, word_bits=(9,))
    elif selected in {"spi_transaction_device", "spi_dma_transaction",
                      "spi_dma_transaction_device"}:
        if item.provider not in {"native", "stm32f407", "gd32f470"}:
            fail("SPI transaction traits require an explicitly reviewed "
                 "provider")
        dma = selected != "spi_transaction_device"
        if dma != (shape.dma or item.mode == "async-model"):
            fail("SPI transaction traits differ from the selected "
                 "provider mode")
        bits = ((8,) if endpoint is not None and
                endpoint.options.model_bytes is not None else (8, 16))
        bound = "UINT16_MAX" if dma else None if provider.MODEL else 256
        limits.update(unit="words", unit_bytes=None, word_bits=bits,
                      max_transfer_units=None if dma else bound,
                      max_segments="NX_SPI_MAX_SEGMENTS",
                      max_segment_units=bound,
                      max_total_units="NX_SPI_MAX_SEGMENTS * UINT16_MAX"
                      if dma else bound)
        formatted["fields"] = {
            **formatted["fields"], "word_bits": 0 if len(bits) > 1 else bits[0]}
    elif selected == "i2c_long_device":
        if item.provider not in {"stm32g474", "stm32h743", "stm32u575"}:
            fail("Long I2C traits require the reviewed v2 provider")
        limits.update(max_transfer_units="NX_I2C_LONG_MAX_MESSAGE_BYTES",
                      max_messages="NX_I2C_MAX_MESSAGES")
    elif selected == "mdio":
        limits.update(unit="registers", unit_bytes=2, max_transfer_units=1,
                      buffer_alignment=1, max_messages=1, word_bits=(16,))
        formatted = dict(fields={}, abort_scope="not_applicable",
                         continuity="not_applicable",
                         timestamp_source="unknown",
                         dma_policy="none", cache_line_bytes=0)
    engines = tuple(sorted({path.split(":", 1)[0]
                            for path in item.values.get("dma", ())}))
    return SelectedContract(
        id=endpoint.id if endpoint is not None else item.id,
        provider=item.provider, controller=item.controller, kind=kind,
        mode=item.mode, public_class=selected, model=provider.MODEL,
        features=("POLLING",) if selected == "mdio" else
        _features(shape, provider.MODEL), dma_engines=engines,
        **formatted, **limits)


def for_configuration(result):
    """Return selected nominal contracts, never a maximum device pool."""
    records = [describe(item) for item in result.controllers]
    controllers = {item.id: item for item in result.controllers}
    records += [describe(controllers[item.controller], item)
                for item in result.devices]
    records += [describe(item, public_class="mdio") for item in
                result.controllers if item.kind in {"eth", "eth_frames"}]
    return tuple(records)


def silicon_policy(result, item=None):
    """Project only the existing selected high-clock revision expectation.

    Ordinary profiles have no completed rule through this projection. Returning
    None never authorizes an unknown revision or executes a hardware check.
    """
    if (item is not None and item in result.controllers and
            item.provider == "stm32h743" and item.kind == "spi" and
            item.mode == "dma"):
        if (result.backend != "baremetal" or
                result["clock_profile"] not in {
                    "hsi64-fixed-cache-off", "hsi64-fixed-cache-on"}):
            fail("Selected SPI DMA silicon expectation requires its exact profile")
        cache = "on" if result["clock_profile"].endswith("-cache-on") else "off"
        return SiliconPolicy(result["part"], result.soc_family,
                             f"spi4-dma-hsi64-cache{cache}-dev450-v2003-trim64",
                             (0x2003,), "selected_spi_dma_mode")
    if item is not None:
        if item not in result.controllers or item.kind != "adc_group":
            fail("Selected silicon profile belongs to no ADC group owner")
        return item.options.admission
    plan = result.clock_plan
    if plan is None:
        return None
    if result.soc_family != "stm32h743":
        fail("Selected silicon clock review has no maintained provider")
    return SiliconPolicy(result["part"], result.soc_family, plan.profile,
                         plan.reviewed_revision_ids, "selected_clock_profile")


def capability(item, model, endpoint=None, *, public_class=None):
    """Adapt the selected contract to the existing optional C capability ABI."""
    record = describe(item, endpoint, public_class=public_class)
    if type(model) is not bool or model != record.model:
        fail("Capability model flag differs from the selected provider")
    flags = " | ".join("NX_FACTORY_CAP_" + name for name in record.features)
    # The existing broad C field holds segment count for enhanced SPI only;
    # the canonical contract keeps messages and segments dimensionally apart.
    messages = (record.max_segments if record.public_class in {
        "spi_transaction_device", "spi_dma_transaction",
        "spi_dma_transaction_device"} else record.max_messages)
    return (flags or "0U", record.max_transfer_units or 0,
            record.unit_bytes or 0, record.buffer_alignment,
            messages or 0)

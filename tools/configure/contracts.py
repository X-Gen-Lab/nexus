"""Maintained authoring decisions; CMake owns component source dependencies."""
from dataclasses import dataclass
from types import MappingProxyType


COMPONENTS = frozenset({
    "owner-observer", "bus-owner", "spi-owner", "spi-async-owner", "spi-dma-owner",
    "uart-owner", "bmp280", "bmp280-spi", "log", "storage",
    "flash-storage", "modbus-rtu", "settings", "smbus",
    "network", "network-loaned-rx", "tls", "mqtt", "mcuboot",
    "st-usb-host", "st-usb-device",
})


@dataclass(frozen=True)
class ControllerContract:
    """Category facts; hardware modes and source targets keep their owners."""
    authored_fields: frozenset[str]
    selected_fields: frozenset[str]
    header: str


# Declaration order preserves factory enumeration and generated declarations.
CONTROLLER_CONTRACTS = MappingProxyType({
    'gpio': ControllerContract(
        frozenset(('binding', 'mode', 'pull', 'output_type', 'speed',
                   'active_high')),
        frozenset(('pull', 'output_type', 'speed', 'active_high')), 'gpio'),
    'uart': ControllerContract(
        frozenset((
            'baud', 'binding', 'fifo', 'frame', 'irq', 'mode', 'rx', 'stream', 'wire',
        )),
        frozenset((
            'baud', 'calls_os', 'fifo', 'frame', 'irq_priority', 'irq_probe',
            'rx_capacity', 'rx_profile', 'stream', 'wire',
        )), 'uart'),
    'spi': ControllerContract(
        frozenset(('binding', 'frames', 'irq', 'max_hz', 'mode')),
        frozenset(('calls_os', 'frames', 'irq_priority', 'max_hz')), 'spi'),
    'i2c': ControllerContract(
        frozenset(('binding', 'irq', 'max_hz', 'mode', 'timing')),
        frozenset(('calls_os', 'irq_priority', 'max_hz', 'timing')), 'i2c'),
    'i2c_target': ControllerContract(
        frozenset(('binding', 'mode', 'irq', 'own_address', 'address_bits',
                   'second_address', 'max_hz', 'irq_latency_us', 'pec', 'alert')),
        frozenset(('irq_priority', 'calls_os', 'own_address', 'address_bits',
                   'second_address', 'max_hz', 'irq_latency_us', 'pec', 'alert')),
        'i2c_target'),
    'flash': ControllerContract(
        frozenset(('binding', 'mode', 'maintenance')),
        frozenset(('maintenance',)), 'flash'),
    'watchdog': ControllerContract(
        frozenset(('binding', 'mode', 'window_ticks')),
        frozenset(('window_ticks',)), 'watchdog'),
    'exti': ControllerContract(
        frozenset(('binding', 'edge', 'event_capacity', 'irq', 'mode', 'pull',
                   'threshold_code')),
        frozenset((
            'calls_os', 'edge', 'event_capacity', 'irq_priority', 'pull',
            'threshold_code',
        )), 'exti'),
    'pwm': ControllerContract(
        frozenset((
            'binding', 'duty_ticks', 'mode', 'period_ticks', 'tick_hz',
        )),
        frozenset(('duty_ticks', 'period_ticks', 'tick_hz')), 'timer'),
    'adc': ControllerContract(
        frozenset((
            'binding', 'channels', 'irq', 'mode', 'reference_mv', 'resolution_bits',
            'sample_times', 'stream', 'timeout_ms', 'trigger',
        )),
        frozenset((
            'calls_os', 'channels', 'irq_priority', 'reference_mv', 'resolution_bits',
            'sample_times', 'stream', 'timeout_ms', 'trigger',
        )), 'adc'),
    'dac': ControllerContract(
        frozenset(('binding', 'mode', 'channels', 'format', 'idle_codes',
                   'output_buffer', 'trigger', 'amplitude', 'irq',
                   'trigger_input', 'recovery_trigger')),
        frozenset(('channels', 'format', 'idle_codes', 'output_buffer',
                   'trigger', 'amplitude', 'irq_priority', 'calls_os',
                   'trigger_input', 'recovery_trigger')), 'dac'),
    'entropy': ControllerContract(frozenset(('binding', 'mode')),
                                  frozenset(), 'entropy'),
    'cipher': ControllerContract(frozenset(('binding', 'mode', 'irq')),
                                  frozenset(('irq_priority', 'calls_os')), 'cipher'),
    'hash': ControllerContract(frozenset(('binding', 'mode', 'irq')),
                                frozenset(('irq_priority', 'calls_os')), 'hash'),
    'display': ControllerContract(
        frozenset(('binding', 'mode', 'irq', 'drain_us', 'timing', 'layers',
                   'memory_window')),
        frozenset(('irq_priority', 'calls_os', 'drain_us', 'timing', 'layers',
                   'memory_window')),
        'display'),
    'image': ControllerContract(
        frozenset(('binding', 'mode', 'irq', 'drain_us', 'memory_window')),
        frozenset(('irq_priority', 'calls_os', 'drain_us', 'memory_window')),
        'image'),
    'crc': ControllerContract(frozenset(('binding', 'mode')),
                              frozenset(), 'crc'),
    'can': ControllerContract(
        frozenset((
            'binding', 'bitrate', 'drain_timeout_us', 'event_capacity',
            'fifo_order', 'irq', 'mode', 'operation', 'poll_limit', 'timing',
        )),
        frozenset((
            'bitrate', 'calls_os', 'drain_timeout_us', 'event_capacity',
            'fifo_order', 'irq_priority', 'operation', 'poll_limit', 'timing',
        )), 'can'),
    'usb_device': ControllerContract(
        frozenset((
            'binding', 'clock_timeout_us', 'drain_timeout_us',
            'event_capacity', 'irq', 'mode', 'poll_limit', 'rx_fifo_words',
            'vbus_sensing', 'remote_wakeup',
            'ulpi_clock_hz', 'electrical_handover', 'engine', 'host', 'power',
        )),
        frozenset((
            'calls_os', 'clock_timeout_us', 'drain_timeout_us',
            'event_capacity', 'irq_priority', 'poll_limit', 'rx_fifo_words',
            'vbus_sensing', 'remote_wakeup',
            'ulpi_clock_hz', 'electrical_handover', 'engine', 'host', 'power',
        )), 'usb_device'),
    'usb_host': ControllerContract(
        frozenset(('binding', 'mode', 'channel_count', 'rx_fifo_words',
                   'nonperiodic_fifo_words', 'periodic_fifo_words', 'engine',
                   'ulpi_clock_hz', 'drain_timeout_us', 'init_timeout_us',
                   'electrical_handover', 'irq', 'pipe_channels')),
        frozenset(('channel_count', 'rx_fifo_words',
                   'nonperiodic_fifo_words', 'periodic_fifo_words', 'engine',
                   'ulpi_clock_hz', 'drain_timeout_us', 'init_timeout_us',
                   'electrical_handover', 'irq_priority', 'calls_os',
                   'pipe_channels')),
        'usb_host'),
    'eth': ControllerContract(
        frozenset((
            'address', 'binding', 'drain_timeout_us', 'init_timeout_us',
            'irq', 'mode', 'rx_count', 'rx_storage_bytes', 'tx_count',
        )),
        frozenset((
            'address', 'calls_os', 'drain_timeout_us', 'init_timeout_us',
            'irq_priority', 'rx_count', 'rx_storage_bytes', 'tx_count',
        )), 'ethernet'),
    'timer_capture': ControllerContract(
        frozenset((
            'binding', 'edge', 'exclusive', 'filter', 'irq', 'mode',
            'tick_hz',
        )),
        frozenset((
            'calls_os', 'edge', 'exclusive', 'filter', 'irq_priority',
            'tick_hz',
        )), 'timer_capture'),
    'advanced_pwm': ControllerContract(
        frozenset((
            'binding', 'break_active_high', 'deadtime_ticks', 'exclusive',
            'idle_complement_high', 'idle_high', 'irq', 'mode',
            'period_ticks', 'tick_hz',
        )),
        frozenset((
            'break_active_high', 'calls_os', 'deadtime_ticks', 'exclusive',
            'idle_complement_high', 'idle_high', 'irq_priority',
            'period_ticks', 'tick_hz',
        )), 'advanced_pwm'),
    'pwm_group': ControllerContract(
        frozenset((
            'binding', 'deadtime_ticks', 'exclusive', 'irq', 'mode',
            'period_ticks', 'tick_hz', 'poll_limit', 'break_active_high', 'idle_high', 'idle_complement_high', 'master_trigger', 'synchronization',
        )),
        frozenset((
            'calls_os', 'deadtime_ticks', 'exclusive', 'irq_priority',
            'period_ticks', 'tick_hz', 'poll_limit', 'break_active_high', 'idle_high', 'idle_complement_high', 'master_trigger', 'synchronization',
        )), 'pwm_group'),
    'timer_pulse': ControllerContract(
        frozenset((
            'binding', 'deadtime_ticks', 'exclusive', 'irq', 'mode',
            'period_ticks', 'tick_hz', 'poll_limit', 'break_active_high', 'idle_high', 'idle_complement_high', 'master_trigger', 'synchronization',
        )),
        frozenset((
            'calls_os', 'deadtime_ticks', 'exclusive', 'irq_priority',
            'period_ticks', 'tick_hz', 'poll_limit', 'break_active_high', 'idle_high', 'idle_complement_high', 'master_trigger', 'synchronization',
        )), 'timer_pulse'),
    'timer_sequence': ControllerContract(
        frozenset((
            'binding', 'deadtime_ticks', 'exclusive', 'irq', 'mode',
            'period_ticks', 'tick_hz', 'burst_budget_hclk_cycles', 'poll_limit', 'break_active_high', 'idle_high', 'idle_complement_high', 'master_trigger', 'synchronization',
        )),
        frozenset((
            'calls_os', 'deadtime_ticks', 'exclusive', 'irq_priority',
            'period_ticks', 'tick_hz', 'burst_budget_hclk_cycles', 'poll_limit', 'break_active_high', 'idle_high', 'idle_complement_high', 'master_trigger', 'synchronization',
        )), 'timer_sequence'),
    'encoder': ControllerContract(
        frozenset((
            'binding', 'counting', 'exclusive', 'filter_a', 'filter_b',
            'invert_a', 'invert_b', 'mode',
        )),
        frozenset((
            'counting', 'exclusive', 'filter_a', 'filter_b', 'invert_a',
            'invert_b',
        )), 'encoder'),
    'pwm_adc': ControllerContract(
        frozenset((
            'binding', 'channels', 'deadtime_ticks', 'exclusive', 'irq',
            'mode', 'period_ticks', 'reference_mv', 'sample_times', 'tick_hz',
            'trigger_ticks',
        )),
        frozenset((
            'calls_os', 'channels', 'deadtime_ticks', 'exclusive',
            'irq_priority', 'period_ticks', 'reference_mv', 'sample_times',
            'tick_hz', 'trigger_ticks',
        )), 'pwm_adc'),
    'can_fd': ControllerContract(
        frozenset((
            'binding', 'bitrate', 'data_bitrate', 'drain_timeout_us',
            'event_capacity', 'exclusive', 'filters', 'irq', 'mode',
            'poll_limit',
        )),
        frozenset((
            'bitrate', 'calls_os', 'data_bitrate', 'drain_timeout_us',
            'event_capacity', 'exclusive', 'filters', 'irq_priority',
            'message_ram_words', 'poll_limit',
        )), 'can_fd'),
    'adc_group': ControllerContract(
        frozenset((
            'binding', 'channels', 'exclusive', 'irq', 'mode', 'period_us',
            'reference_mv', 'sample_times', 'operation', 'trigger',
            'watchdog', 'resolution_bits', 'delay_cycles', 'drain_timeout_us',
            'max_service_gap_us', 'injected',
        )),
        frozenset((
            'calls_os', 'channels', 'exclusive', 'irq_priority', 'period_us',
            'reference_mv', 'sample_times', 'operation', 'trigger',
            'watchdog', 'resolution_bits', 'delay_cycles', 'drain_timeout_us',
            'max_service_gap_us', 'injected',
        )), 'adc_group'),
    'eth_frames': ControllerContract(
        frozenset((
            'address', 'binding', 'drain_timeout_us', 'init_timeout_us',
            'irq', 'mode', 'rx_count', 'rx_storage_bytes', 'tx_count',
            'checksum_offload',
        )),
        frozenset((
            'address', 'calls_os', 'drain_timeout_us', 'init_timeout_us',
            'irq_priority', 'rx_count', 'rx_storage_bytes', 'tx_count',
            'checksum_offload',
        )), 'ethernet_frames'),
    'rtc': ControllerContract(
        frozenset(('binding', 'mode', 'allow_set', 'timeout_us', 'poll_limit',
                   'initial', 'lse_bypass', 'periodic_wakeup')),
        frozenset(('allow_set', 'timeout_us', 'poll_limit', 'initial',
                   'lse_bypass', 'periodic_wakeup')), 'rtc'),
    'lptim': ControllerContract(
        frozenset(('binding', 'irq', 'mode', 'tick_hz')),
        frozenset(('tick_hz',)), 'lptim'),
    'block': ControllerContract(
        frozenset(('binding', 'mode', 'bus_width', 'max_blocks',
                   'poll_budget', 'stop_timeout_us', 'irq', 'protocol', 'speed')),
        frozenset(('bus_width', 'max_blocks', 'poll_budget',
                   'stop_timeout_us', 'irq_priority', 'calls_os', 'protocol', 'speed')),
        'block'),
    'capture': ControllerContract(
        frozenset(('binding', 'mode', 'irq', 'pixel_clock_hz', 'poll_limit',
                   'frame_divisor', 'pclk_falling', 'hsync_high', 'vsync_high',
                   'codes', 'crop', 'max_service_gap_us')),
        frozenset(('irq_priority', 'calls_os', 'pixel_clock_hz', 'poll_limit',
                   'frame_divisor', 'pclk_falling', 'hsync_high', 'vsync_high',
                   'codes', 'crop', 'max_service_gap_us')), 'capture'),
    'audio': ControllerContract(
        frozenset(('binding', 'mode', 'irq', 'standard', 'sample_bits',
                   'slot_bits', 'mclk', 'cpol_high', 'poll_limit',
                   'sample_rate_hz', 'pll_n', 'pll_r', 'max_error_ppm',
                   'max_sample_rate_hz', 'max_service_gap_us', 'slots',
                   'frame_sync_bits', 'first_bit_offset', 'frame_sync_high',
                   'frame_sync_before', 'frame_sync_channels', 'first_lsb',
                   'sample_rising', 'no_divider', 'mckdiv', 'max_bit_clock_hz',
                   'active_slots')),
        frozenset(('irq_priority', 'calls_os', 'standard', 'sample_bits',
                   'slot_bits', 'mclk', 'cpol_high', 'poll_limit',
                   'sample_rate_hz', 'pll_n', 'pll_r', 'max_error_ppm',
                   'max_sample_rate_hz', 'max_service_gap_us', 'slots',
                   'frame_sync_bits', 'first_bit_offset', 'frame_sync_high',
                   'frame_sync_before', 'frame_sync_channels', 'first_lsb',
                   'sample_rising', 'no_divider', 'mckdiv', 'max_bit_clock_hz',
                   'active_slots')), 'audio'),
    'external_bus': ControllerContract(
        frozenset(('binding', 'mode')), frozenset(), 'external_bus'),
})

CONTROLLER_KINDS = tuple(CONTROLLER_CONTRACTS)
# The graph checks the record envelope before its exact kind/mode contract.
CONTROLLER_SELECTION_FIELDS = frozenset().union(
    *(contract.selected_fields for contract in CONTROLLER_CONTRACTS.values()))


ENDPOINT_FIELDS = MappingProxyType({
    "spi": frozenset(("controller", "driver", "cs_binding", "mode", "max_hz",
                      "model_bytes", "lsb_first")),
    "i2c": frozenset(("controller", "driver", "address", "model_bytes", "address_bits")),
})
EXTERNAL_ENDPOINT_FIELDS = frozenset((
    "controller", "binding", "read_timing", "write_timing"))
EXTERNAL_ENDPOINT_KINDS = ("memory_window", "parallel16", "flash_async", "nand",
                           "mapped_storage")
SERIAL_FLASH_FIELDS = frozenset(("controller", "binding", "mode", "parameters"))
EXTERNAL_ENDPOINT_REQUIRED = MappingProxyType({
    "mapped_storage": SERIAL_FLASH_FIELDS,
    "memory_window": EXTERNAL_ENDPOINT_FIELDS | {"mode"},
    "parallel16": EXTERNAL_ENDPOINT_FIELDS,
    "flash_async": EXTERNAL_ENDPOINT_FIELDS | {"parameters", "nor"},
    "nand": frozenset(("controller", "binding", "parameters", "geometry", "ready_delay_us")),
})
# Endpoint faces are reviewed personalities, never independent controllers.
FACT_INTERFACE_KINDS = frozenset(CONTROLLER_KINDS) | set(EXTERNAL_ENDPOINT_KINDS)
USB_ENDPOINT_FIELDS = frozenset(("controller", "number", "direction",
                                 "transfer", "max_packet", "fifo_words"))
ENDPOINT_SELECTION_FIELDS = frozenset().union(
    *ENDPOINT_FIELDS.values(), USB_ENDPOINT_FIELDS,
    EXTERNAL_ENDPOINT_FIELDS) | {"authored_kind", "mode", "parameters", "nor", "geometry", "ready_delay_us"}

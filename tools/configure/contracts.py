"""Maintained authoring decisions; CMake owns component source dependencies."""
from dataclasses import dataclass
from types import MappingProxyType


COMPONENTS = frozenset({
    "owner-observer", "bus-owner", "spi-owner", "spi-async-owner", "spi-dma-owner",
    "uart-owner", "bmp280", "bmp280-spi", "log", "storage",
    "flash-storage", "modbus-rtu",
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
        frozenset(('binding', 'mode')),
        frozenset(()), 'gpio'),
    'uart': ControllerContract(
        frozenset((
            'baud', 'binding', 'fifo', 'frame', 'irq', 'mode', 'rx', 'stream',
        )),
        frozenset((
            'baud', 'calls_os', 'fifo', 'frame', 'irq_priority', 'irq_probe',
            'rx_capacity', 'rx_profile', 'stream',
        )), 'uart'),
    'spi': ControllerContract(
        frozenset(('binding', 'irq', 'max_hz', 'mode')),
        frozenset(('calls_os', 'irq_priority', 'max_hz')), 'spi'),
    'i2c': ControllerContract(
        frozenset(('binding', 'irq', 'max_hz', 'mode', 'timing')),
        frozenset(('calls_os', 'irq_priority', 'max_hz', 'timing')), 'i2c'),
    'flash': ControllerContract(
        frozenset(('binding', 'mode')),
        frozenset(()), 'flash'),
    'watchdog': ControllerContract(
        frozenset(('binding', 'mode')),
        frozenset(()), 'watchdog'),
    'exti': ControllerContract(
        frozenset(('binding', 'edge', 'event_capacity', 'irq', 'mode')),
        frozenset((
            'calls_os', 'edge', 'event_capacity', 'irq_priority',
        )), 'exti'),
    'pwm': ControllerContract(
        frozenset((
            'binding', 'duty_ticks', 'mode', 'period_ticks', 'tick_hz',
        )),
        frozenset(('duty_ticks', 'period_ticks', 'tick_hz')), 'timer'),
    'adc': ControllerContract(
        frozenset((
            'binding', 'channels', 'irq', 'mode', 'reference_mv',
            'sample_times', 'stream', 'timeout_ms',
        )),
        frozenset((
            'calls_os', 'channels', 'irq_priority', 'reference_mv',
            'sample_times', 'stream', 'timeout_ms',
        )), 'adc'),
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
            'vbus_sensing',
        )),
        frozenset((
            'calls_os', 'clock_timeout_us', 'drain_timeout_us',
            'event_capacity', 'irq_priority', 'poll_limit', 'rx_fifo_words',
            'vbus_sensing',
        )), 'usb_device'),
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
            'period_ticks', 'tick_hz',
        )),
        frozenset((
            'calls_os', 'deadtime_ticks', 'exclusive', 'irq_priority',
            'period_ticks', 'tick_hz',
        )), 'pwm_group'),
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
            'reference_mv', 'sample_times', 'silicon_profile', 'trigger',
            'watchdog',
        )),
        frozenset((
            'calls_os', 'channels', 'exclusive', 'irq_priority', 'period_us',
            'reference_mv', 'sample_times', 'silicon_profile', 'trigger',
            'watchdog',
        )), 'adc_group'),
    'eth_frames': ControllerContract(
        frozenset((
            'address', 'binding', 'drain_timeout_us', 'init_timeout_us',
            'irq', 'mode', 'rx_count', 'rx_storage_bytes', 'tx_count',
        )),
        frozenset((
            'address', 'calls_os', 'drain_timeout_us', 'init_timeout_us',
            'irq_priority', 'rx_count', 'rx_storage_bytes', 'tx_count',
        )), 'ethernet_frames'),
    'rtc': ControllerContract(
        frozenset(('binding', 'mode')),
        frozenset(()), 'rtc'),
    'lptim': ControllerContract(
        frozenset(('binding', 'irq', 'mode', 'tick_hz')),
        frozenset(('tick_hz',)), 'lptim'),
})

CONTROLLER_KINDS = tuple(CONTROLLER_CONTRACTS)
# The graph checks the record envelope before its exact kind/mode contract.
CONTROLLER_SELECTION_FIELDS = frozenset().union(
    *(contract.selected_fields for contract in CONTROLLER_CONTRACTS.values()))

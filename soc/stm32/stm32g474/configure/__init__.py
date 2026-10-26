"""Explicit family contract and static-construction exports."""
from .contract import (
    PARTS,
    i2c_timing_clock,
    PUBLIC_FORMATS,
    ADC_GROUP,
    ADC_PIN_CHANNELS,
    CAN_CONTROLLERS,
    CLOCK_PROFILES,
    CONTROLLER_INTERFACES,
    CPU_ABI,
    CPU_FEATURES,
    DMA_TOPOLOGY,
    DMA_VECTOR,
    DWT_CYCCNT,
    ENDPOINT_POLICY,
    ENUM_ABI,
    ETH_CONTROLLER,
    EXTERNAL_IRQ_COUNT,
    FACT_REFERENCES,
    FLASH_INPUT_ALIGNMENT,
    FLASH_GEOMETRY,
    FLASH_PROGRAM_UNIT,
    FREERTOS_PORT,
    IRQ_PRIORITY_BITS,
    LIMITS,
    MODEL,
    PREFIX,
    PWM_SHARED,
    SPI_REQUIRES_ENDPOINT,
    TIMEBASE_ALARM,
    USB_CONTROLLER,
    _MODES,
    builtin_cs,
    configuration_header,
    dma_irq,
    mode_fields,
    mode_implementation,
    shares_irq,
    validate_controller_set,
    validate_dma,
    validate_dma_resource,
    validate_endpoint_clock,
    validate_mode_context,
    validate_selection,
    validate_user_io_context,
)
from .bindings import constructor
from .emission import (includes, instance_declarations, implementation, operations,
                       state_declarations, shared_irqs, stop_system,
                       start_system, start_resources)
from tools.configure.clocks import normalize as normalize_clock, plan as clock_plan
from .preflight import PREFLIGHT_CALL, selected_preflight
from . import gpio as GPIO_PHASE
from .integration import TIMER_ADVANCED
from . import coupled as COUPLED_IO
from .integration import validate_pin_states_selection

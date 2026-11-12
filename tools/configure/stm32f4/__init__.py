"""Explicit family contract and static-construction exports."""
from .contract import (
    CLASSIC_CAN_MAILBOXES,
    UART_FORMATS,
    UART_TX_VALIDATORS,
    STREAM_GEOMETRY,
    PARTS,
    PIN_ELECTRICAL,
    PWM_PIN_CHANNELS,
    PUBLIC_FORMATS,
    ADC_PIN_CHANNELS,
    ADC_INTERNAL_CHANNELS,
    ADC_TRIGGERS,
    DAC_PIN_CHANNELS,
    ADC_GROUP,
    CAN_CLOCK_HZ,
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
    FACT_REFERENCES,
    FLASH_INPUT_ALIGNMENT,
    FLASH_PROGRAM_UNIT,
    FREERTOS_PORT,
    IRQ_PRIORITY_BITS,
    LIMITS,
    MODEL,
    OFFSET_IMAGE_ALIGNMENT,
    OFFSET_IMAGE_VECTOR_SYMBOL,
    PREFIX,
    PWM_SHARED,
    SPI_REQUIRES_ENDPOINT,
    TIMEBASE_ALARM,
    USB_CONTROLLER,
    _FIXED_DMA,
    _MODE_IMPLEMENTATIONS,
    builtin_cs,
    configuration_header,
    dma_irq,
    mode_fields,
    mode_implementation,
    shares_irq,
    validate_controller_set,
    validate_clock_policy,
    validate_dma,
    validate_dma_resource,
    validate_endpoint_clock,
    validate_mode_context,
    validate_selection,
)
from .bindings import constructor
from .emission import (includes, instance_declarations, implementation, operations,
                       state_declarations, shared_irqs, platform_declarations,
                       platform_system_start, platform_resources_acquire,
                       platform_resources_release, platform_system_release,
                       platform_resources_resume)
from .emission import platform_power_controllers
from .clocks import normalize as normalize_clock, plan as clock_plan
from .clocks import controller_hz as controller_clock_hz
from .clocks import validate_consumers as validate_clock_consumers
from .clocks import timer_domain as timer_clock_domain
from . import gpio as GPIO_PHASE
from . import sram as EXTERNAL_MEMORY
from .hardware import (CONTROLLER_BINDINGS, DMA_IRQS,
                       SECONDARY_FACTS, DAC_DMA_ROUTES)
from .integration import USB_DEVICE, ETHERNET, TIMER_ADVANCED
from .integration import CAN_INTEGRATION
from . import external_bus as EXTERNAL_BUS
from .i2c_timing import selected_format

from .capture_audio import resolve_fields as capture_audio_fields


def silicon_policy(result, item=None):
    """Project the system's reviewed C admission without observing a chip."""
    from tools.configure.selected_contracts import SiliconPolicy
    if item is not None or result.clock_plan is None:
        return None
    return SiliconPolicy(
        part=result.part, provider=result.soc_family,
        profile=result.clock_plan.profile,
        reviewed_revision_ids=result.clock_plan.reviewed_revision_ids,
        review_scope=(
            result.clock_plan.provenance + "; mode exclusions and external "
            "constraints remain in the selected review. The C silicon rule "
            "uses the selected SDK group; physical cut behavior has not been "
            "observed."))

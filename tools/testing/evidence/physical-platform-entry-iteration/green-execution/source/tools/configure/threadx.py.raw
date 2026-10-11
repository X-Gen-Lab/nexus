"""Strict initial ThreadX policy; no FreeRTOS feature/profile substitution."""
from types import MappingProxyType

try:
    from .ir import ThreadXKernelProfileIR
    from .providers.common import fail, integer
except ImportError:
    from ir import ThreadXKernelProfileIR
    from providers.common import fail, integer


FIELDS = frozenset({"tick_source", "tick_hz", "max_priorities",
                    "interrupt_mask", "syscall_priority"})
PLATFORMS = MappingProxyType({
    "native-reference": "native",
    "stm32f407zg-qiming-v31": "stm32f407",
    "stm32f407ve-sky-qingchun": "stm32f407",
    "gd32f470zg-liangshan": "gd32f470",
})


def validate_platform(board, family):
    """A CPU port does not authorize arbitrary SoC/Board startup wiring."""
    if PLATFORMS.get(board) != family:
        fail("Unsupported maintained ThreadX platform")


def resolve(choices, cpu, clock_hz):
    """Bind immutable static-object policy to one actual reviewed GNU port."""
    if cpu.backend != "threadx" or cpu.arch not in {"native", "cortex-m4"}:
        fail("Unsupported maintained ThreadX CPU port")
    if choices is None:
        choices = {}
    if not isinstance(choices, dict):
        fail("ThreadX OS policy: expected object")
    unknown = choices.keys() - FIELDS
    if unknown:
        fail(f"ThreadX OS policy: unknown {sorted(unknown)}")
    integer(clock_hz, 1, 0xffffffff, "CPU clock Hz")
    native = cpu.arch == "native"
    source = choices.get("tick_source", "host" if native else "systick")
    if native:
        if source != "host":
            fail("ThreadX Linux requires the reviewed monotonic host Tick")
    elif not isinstance(source, str) or source not in {"systick", "external"}:
        fail("Unsupported ThreadX tick source")
    tick_hz = choices.get("tick_hz", 1000)
    integer(tick_hz, 1, 1000000, "ThreadX Tick Hz")
    if native and 1000000000 % tick_hz:
        fail("ThreadX host Tick needs an exact integer nanosecond period")
    priorities = choices.get("max_priorities", 32)
    integer(priorities, 32, 32, "ThreadX priorities")
    mask = choices.get("interrupt_mask", "none" if native else "basepri")
    if not isinstance(mask, str) or mask not in (
            {"none"} if native else {"basepri", "primask"}):
        fail("Unsupported ThreadX interrupt mask")
    ceiling = choices.get("syscall_priority", 5 if mask == "basepri" else 0)
    integer(ceiling, 1 if mask == "basepri" else 0,
            cpu.irq.maximum_priority if mask == "basepri" else 0,
            "ThreadX syscall priority")
    reload = None
    if source == "systick":
        cycles, remainder = divmod(clock_hz, tick_hz)
        if remainder or not 2 <= cycles <= 0x1000000:
            fail("SysTick needs an exact frequency and reload in [1, 0xffffff]")
        reload = cycles - 1
    encoded = ceiling << (8 - cpu.irq.priority_bits) if mask == "basepri" else 0
    definitions = {
        "NEXUS_OS_TICK_HZ": tick_hz,
        "NEXUS_OS_EXTERNAL_TICK": int(source == "external"),
        "NEXUS_OS_HOST_TICK": int(source == "host"),
        "NEXUS_OS_SYSTICK_RELOAD": reload if reload is not None else 0,
        "NEXUS_OS_MAX_PRIORITIES": priorities,
        "NEXUS_OS_MIN_STACK_BYTES": 200,
        "NEXUS_OS_STACK_ALIGNMENT_BYTES": 8,
        "NEXUS_THREADX_TIMER_PROCESS_IN_ISR": 1,
        "NEXUS_THREADX_BASEPRI": encoded,
        "NEXUS_IRQ_KERNEL_POLICY": {"none": 0, "primask": 1, "basepri": 2}[mask],
    }
    return ThreadXKernelProfileIR.from_validated({
        "backend": "threadx", "profile": "static", "tick_source": source,
        "tick_hz": tick_hz, "tick_reload": reload,
        "max_priorities": priorities, "syscall_priority": ceiling,
        "interrupt_mask": mask, "kernel_policy": mask,
        "kernel_port": "linux/gnu" if native else "cortex_m4/gnu",
        "timer_process_in_isr": True, "min_stack_bytes": 200,
        "stack_alignment_bytes": 8, "definitions": definitions,
    })


def editor_schema():
    """Only explicit fields accepted by resolve are editor-authorable."""
    return {"type": "object", "additionalProperties": False, "properties": {
        "tick_source": {"enum": ["host", "systick", "external"]},
        "tick_hz": {"type": "integer", "minimum": 1, "maximum": 1000000},
        "max_priorities": {"const": 32},
        "interrupt_mask": {"enum": ["none", "basepri", "primask"]},
        "syscall_priority": {"type": "integer", "minimum": 0, "maximum": 255},
    }}

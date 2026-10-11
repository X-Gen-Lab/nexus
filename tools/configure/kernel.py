"""One bounded kernel policy authority; CPU/IRQ capabilities remain chip facts.

The profiles configure the locked static-only FreeRTOS ports. They create no
tasks, queues, Tick hardware, trace buffers or product recovery policy.
"""
from types import MappingProxyType

try:
    from .ir import CpuProfileIR, KernelProfileIR
    from .providers.common import fail, integer
except ImportError:
    from ir import CpuProfileIR, KernelProfileIR
    from providers.common import fail, integer


_STANDARD = {"tick_source": "systick", "tick_hz": 1000,
             "max_priorities": 8, "idle_stack_words": 128,
             "max_task_name_len": 16, "mutexes": True,
             "counting_semaphores": True, "task_notifications": True,
             "notification_slots": 1, "trace": False,
             "runtime_stats": False, "tickless": False,
             "memory_protection": False, "system_call_stack_words": 128,
             "security_model": "single", "secure_idle_stack_bytes": 128}
PROFILES = MappingProxyType({
    "standard": MappingProxyType(_STANDARD),
    "minimal": MappingProxyType({**_STANDARD, "max_priorities": 4,
                                 "max_task_name_len": 8, "mutexes": False,
                                 "counting_semaphores": False,
                                 "task_notifications": False}),
    "diagnostic": MappingProxyType({**_STANDARD, "trace": True}),
    "lowpower": MappingProxyType({**_STANDARD, "tickless": True}),
})
FIELDS = frozenset({"profile", "syscall_priority", *_STANDARD})
BOOL_FIELDS = frozenset({"mutexes", "counting_semaphores", "task_notifications",
                         "trace", "runtime_stats", "tickless",
                         "memory_protection"})
# Minimum startup capacity includes the top-of-array element and its worst-case
# downward 8-byte alignment. It is not an application stack/high-water budget.
# The locked CM0 saves EXC_RETURN (17-word decrement); CM3 uses 16 words;
# FP CM4/CM7 use 17; MPU-disabled/NTZ v8 ports add PSPLIM (18).
PORT_MINIMUM_WORDS = MappingProxyType({
    "GCC/ARM_CM0": 20, "GCC/ARM_CM3": 18,
    "GCC/ARM_CM4F": 20, "GCC/ARM_CM7/r0p1": 20,
    "nexus/ARM_CM7_integer": 18,
    **{f"GCC/ARM_CM{core}_NTZ/non_secure": 20
       for core in (23, 33, 55, 85)},
})


def default_syscall(has_basepri, priority_bits):
    """Preserve the reviewed default while keeping policy out of CPU facts."""
    return (10 if priority_bits == 8 else 5) if has_basepri else 0


def resolve(choices, cpu, clock_hz):
    """Validate kernel policy against the exact CPU Port and clock contract."""
    if cpu.backend != "freertos":
        if choices is not None:
            fail("OS policy requires the FreeRTOS backend")
        return None
    if choices is None:
        choices = {}
    if not isinstance(choices, dict):
        fail("OS policy: expected object")
    unknown = choices.keys() - FIELDS
    if unknown:
        fail(f"OS policy: unknown {sorted(unknown)}")
    profile = choices.get("profile", "standard")
    if not isinstance(profile, str) or profile not in PROFILES:
        fail("Unsupported maintained OS profile")
    options = {**PROFILES[profile], **choices}
    integer(clock_hz, 1, context="CPU clock Hz")
    for name in BOOL_FIELDS:
        if type(options[name]) is not bool:
            fail(f"OS {name}: boolean required")
    limits = {"tick_hz": (1, 1000000), "max_priorities": (1, 32),
              "idle_stack_words": (1, 65536), "max_task_name_len": (1, 64),
              "notification_slots": (1, 8),
              "system_call_stack_words": (64, 65536)}
    for name, (low, high) in limits.items():
        integer(options[name], low, high, "OS " + name)
    port = cpu.irq.kernel_port
    security = options["security_model"]
    if not isinstance(security, str) or security not in {"single", "split"}:
        fail("Unsupported OS security model")
    if security == "split" and (cpu.arch not in {
            "cortex-m23", "cortex-m33", "cortex-m55", "cortex-m85"} or
            cpu["cpu"]["security"] != "nonsecure"):
        fail("Split kernel security requires explicit v8-M Nonsecure CPU facts")
    if security == "split":
        port = f"GCC/ARM_CM{cpu.arch[8:]}/non_secure"
        integer(options["secure_idle_stack_bytes"], 64, 65536,
                "Secure idle stack bytes")
        if options["secure_idle_stack_bytes"] % 8:
            fail("Secure idle stack budget needs 8-byte alignment")
    elif "secure_idle_stack_bytes" in choices:
        fail("Secure idle stack budget requires split kernel security")
    else:
        options["secure_idle_stack_bytes"] = 0
    protected = options["memory_protection"]
    if protected and security == "split":
        fail("MPU and split-security kernel combination is not maintained")
    if options["system_call_stack_words"] % 2:
        fail("System call stack must have 8-byte alignment")
    if protected:
        if not cpu.definitions["NEXUS_CPU_MPU_REGIONS"]:
            fail("MPU kernel policy requires an explicit MPU region count")
        if cpu.arch in {"cortex-m3", "cortex-m4"}:
            port = ("GCC/ARM_CM4_MPU" if cpu.fpu != "none" else
                    "GCC/ARM_CM3_MPU")
        elif cpu.arch not in {
                "cortex-m23", "cortex-m33", "cortex-m55", "cortex-m85"}:
            fail("Unsupported maintained MPU kernel tuple")
    minimum = PORT_MINIMUM_WORDS.get(cpu.irq.kernel_port)
    if minimum is None:
        fail("Unsupported kernel startup frame contract")
    if security == "split":
        minimum = 22
    if options["idle_stack_words"] < minimum or options["idle_stack_words"] % 2:
        fail("Idle stack must fit the reviewed initial frame and 8-byte alignment")
    source = options["tick_source"]
    if not isinstance(source, str) or source not in {"systick", "external"}:
        fail("Unsupported OS tick source")
    tick_hz = options["tick_hz"]
    reload = None
    if source == "systick":
        cycles, remainder = divmod(clock_hz, tick_hz)
        if remainder or not 2 <= cycles <= 0x1000000:
            fail("SysTick needs an exact frequency and reload in [1, 0xffffff]")
        reload = cycles - 1
    if options["tickless"] and 1000000 % tick_hz:
        fail("Tickless requires an exact integer microsecond Tick period")
    ceiling = options.get("syscall_priority", default_syscall(
        cpu.has_basepri, cpu.irq.priority_bits))
    integer(ceiling, 1 if cpu.has_basepri else 0,
            cpu.irq.maximum_priority if cpu.has_basepri else 0,
            "OS syscall priority")
    policy = "basepri" if cpu.has_basepri else "primask"
    definitions = {
        "NEXUS_OS_TICK_HZ": tick_hz,
        "NEXUS_OS_EXTERNAL_TICK": int(source == "external"),
        "NEXUS_OS_SYSTICK_RELOAD": reload if reload is not None else 0,
        "NEXUS_OS_MAX_PRIORITIES": options["max_priorities"],
        "NEXUS_OS_IDLE_STACK_WORDS": options["idle_stack_words"],
        "NEXUS_OS_MAX_TASK_NAME_LEN": options["max_task_name_len"],
        "NEXUS_OS_MUTEXES": int(options["mutexes"]),
        "NEXUS_OS_COUNTING_SEMAPHORES": int(options["counting_semaphores"]),
        "NEXUS_OS_TASK_NOTIFICATIONS": int(options["task_notifications"]),
        "NEXUS_OS_NOTIFICATION_SLOTS": options["notification_slots"],
        "NEXUS_OS_TRACE": int(options["trace"]),
        "NEXUS_OS_RUNTIME_STATS": int(options["runtime_stats"]),
        "NEXUS_OS_TICKLESS": int(options["tickless"]),
        "NEXUS_OS_MEMORY_PROTECTION": int(protected),
        "NEXUS_OS_SYSTEM_CALL_STACK_WORDS": options["system_call_stack_words"],
        "NEXUS_OS_TRUSTZONE": int(security == "split"),
        "NEXUS_OS_SECURE_IDLE_STACK_BYTES": options["secure_idle_stack_bytes"],
        "NEXUS_OS_MIN_STACK_WORDS": minimum,
        "NEXUS_OS_STACK_ALIGNMENT_BYTES": 8,
        "NEXUS_IRQ_KERNEL_POLICY": 2 if cpu.has_basepri else 1,
    }
    record = {**options, "profile": profile, "tick_reload": reload,
              "syscall_priority": ceiling, "kernel_policy": policy,
              "kernel_port": port,
              "min_stack_words": minimum, "stack_alignment_bytes": 8,
              "definitions": definitions}
    return KernelProfileIR.from_validated(record)


def bind(cpu, kernel):
    """Attach selected logical syscall policy without mutating physical facts."""
    if kernel is None:
        return cpu
    record = cpu.to_dict()
    record["irq"]["syscall_priority"] = kernel.syscall_priority
    record["irq"]["kernel_port"] = kernel.kernel_port
    return CpuProfileIR.from_validated(record)


def header_lines(kernel):
    return ([] if kernel is None else
            [f"#define {key} {value}u"
             for key, value in kernel.definitions.items()
             if key != "NEXUS_IRQ_KERNEL_POLICY"])


def selection_lines(kernel):
    return ([] if kernel is None else
            [f'set(NEXUS_OS_PROFILE "{kernel.profile}")',
             'set(NEXUS_OS_SELECTION_VARIABLES "NEXUS_OS_PROFILE;' +
             ';'.join(key for key in kernel.definitions
                      if key != "NEXUS_IRQ_KERNEL_POLICY") + '")',
             *[f'set({key} "{value}")'
               for key, value in kernel.definitions.items()
               if key != "NEXUS_IRQ_KERNEL_POLICY"]])


def editor_schema():
    """Describe the same bounded choices used by the resolver."""
    properties = {name: {"type": "boolean"} for name in sorted(BOOL_FIELDS)}
    for name, low, high in (("tick_hz", 1, 1000000),
                            ("max_priorities", 1, 32),
                            ("idle_stack_words", 1, 65536),
                            ("max_task_name_len", 1, 64),
                            ("notification_slots", 1, 8),
                            ("system_call_stack_words", 64, 65536),
                            ("secure_idle_stack_bytes", 64, 65536),
                            ("syscall_priority", 0, 255)):
        properties[name] = {"type": "integer", "minimum": low, "maximum": high}
    properties["profile"] = {"enum": sorted(PROFILES)}
    properties["tick_source"] = {"enum": ["external", "systick"]}
    properties["security_model"] = {"enum": ["single", "split"]}
    return {"type": "object", "properties": properties,
            "additionalProperties": False}

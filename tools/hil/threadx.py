"""Independent ThreadX acquisition policy for maintained Native/M4 ports.

These contracts describe retained station observations. They neither operate
equipment nor extend the configure authority's maintained CPU/Board scope.
"""

from pathlib import Path

from tools.configure import cpu, kernel, threadx as policy
from tools.evidence.common import EvidenceError, fields, identifier, load_json
from tools.evidence.identity import canonical_digest
from tools.hil.admit import require


CASES = {
    "task": "ThreadX: observe static threads, priority/threshold and dispatch",
    "irq_wake": "ThreadX: record ISR-safe wake and resumed thread timestamps",
    "context": "ThreadX: compare selected M4 integer and optional FP banks",
    "stack": "ThreadX: watermark every selected thread and exception MSP",
    "deadline": "ThreadX: record Core monotonic deadlines and actual returns",
    "lifecycle": "ThreadX: observe cancel/drain/join and retain live loans",
    "fail_stop": "ThreadX: retain observer, incoming masks and loan state",
}
WORKLOAD_CASES = frozenset("threadx_" + name for name in CASES)
MAX_VALUE = 2 ** 64 - 1


def contract(resolved):
    """Recompute the selected kernel policy with the configure authority."""
    require(resolved.get("backend") == "threadx", "ThreadX backend required")
    try:
        board, family = resolved["board"], resolved["soc_family"]
        policy.validate_platform(board, family)
        # Read the owned Board identity instead of copying a chip/Board table.
        boards = [load_json(path) for path in
                  (Path(__file__).resolve().parents[2] / "boards").glob(
                      "*/board.json")]
        require(any(item.get("id") == board and
                    item.get("soc_family") == family and
                    item.get("soc") == resolved.get("part") for item in boards),
                "ThreadX exact maintained Board/part identity required")
        irq = resolved["irq"]
        profile = cpu.resolve(resolved["cpu"], {name: irq[name] for name in
                              ("priority_bits", "external_count")}, "threadx")
        selected = resolved["kernel_profile"]
        require(isinstance(selected, dict), "ThreadX kernel profile required")
        choices = {name: selected[name] for name in policy.FIELDS}
        expected = kernel.resolve(
            choices, profile, resolved["clock"]["core_hz"])
        require(canonical_digest(selected) ==
                canonical_digest(expected.to_dict()),
                "ThreadX kernel profile differs from authoritative resolution")
        require(canonical_digest(resolved["cpu_profile"]) ==
                canonical_digest(kernel.bind(profile, expected).to_dict()),
                "ThreadX CPU/IRQ ABI differs from selected kernel policy")
    except (ValueError, KeyError, TypeError, OSError) as error:
        raise EvidenceError(
            "invalid maintained ThreadX contract: " + str(error)) from error
    return {
        "backend": "threadx",
        "kernel_profile_sha256": canonical_digest(selected),
        "kernel_port": expected.kernel_port, "max_priorities": 32,
        "priority_order": "lower_number_higher", "timer_process_in_isr": True,
        "tick_source": expected.tick_source,
        "interrupt_mask": expected.interrupt_mask,
        "syscall_priority": expected.syscall_priority,
    }


def selection(name, resolved):
    """Native support stays software-only; physical plans require exact M4."""
    require(name in WORKLOAD_CASES, "unknown ThreadX acquisition case")
    if resolved.get("backend") != "threadx":
        return "unsupported", ["selected_threadx_backend_required"]
    contract(resolved)
    if resolved["cpu"]["arch"] == "native":
        return "unsupported", ["physical_threadx_m4_target_required"]
    return "selected", []


def workload_definitions(define):
    """Publish independent authoring obligations to the existing planner."""
    specifications = {
        "task": (53, "Run all selected static threads under reviewed load",
                 ("thread_inventory_priority_threshold",
                  "dispatch_expected_observed",
                  "per_thread_iterations_start_finish"),
                 {"iterations": "count", "task_mismatches": "count"}),
        "irq_wake": (49, "Wake selected threads using the ISR-safe adapter",
                     ("irq_service_priority",
                      "irq_latch_thread_resume_timestamps",
                      "wake_sequence_loss"),
                     {"wake_latency": "us", "wake_loss": "count"}),
        "context": (25, "Preempt distinct selected M4 register patterns",
                    ("r4_r11_expected_observed",
                     "selected_fp_expected_observed",
                     "interrupt_preemption_load"),
                    {"context_mismatches": "count"}),
        "stack": (27, "Watermark every selected static thread and MSP",
                  ("complete_thread_msp_inventory", "capacity_peak_bytes"),
                  {"minimum_headroom": "bytes"}),
        "deadline": (26, "Compare Core monotonic deadlines with actual returns",
                     ("deadline_return_timestamps",
                      "selected_tick_source_load"),
                     {"deadline_lateness": "us"}),
        "lifecycle": (28, "Repeat cancellation, settlement and joined reclaim",
                      ("iterations_admitted_settled_joined",
                       "retained_stale_accesses"),
                      {"iterations": "count", "retained_borrows": "count"}),
        "fail_stop": (30, "Trigger assert/stack overflow in an isolated rig",
                      ("trigger_safe_output_timestamps",
                       "fault_observer_record",
                       "incoming_masks", "retained_loans", "terminal_progress"),
                      {"response_time": "us", "unsafe_outputs": "count"}),
    }
    return {"threadx_" + name: define(
        item[0], (), item[1], item[2], ("debug_probe", "threadx_trace_capture"),
        ("exact_threadx_profile", "reviewed_irq_ceiling",
         "outputs_disconnected",
         "retained_loans_never_force_reclaimed"), item[3])
            for name, item in specifications.items()}


def _integer(value, label, minimum=0, maximum=MAX_VALUE):
    require(type(value) is int and minimum <= value <= maximum,
            "bounded ThreadX " + label + " required")
    return value


def validate_context(context, resolved, workload):
    """Bind task/IRQ observations to explicit reviewed ThreadX semantics."""
    require(canonical_digest(context["backend_contract"]) ==
            canonical_digest(contract(resolved)),
            "ThreadX acquisition backend/profile contract mismatch")
    require(resolved["cpu"]["arch"] == "cortex-m4",
            "ThreadX Native observations cannot qualify MCU hardware")
    require(context["cpu_clock_hz"] == resolved["clock"]["core_hz"],
            "ThreadX observed CPU clock differs from exact selected clock")
    require(WORKLOAD_CASES.issubset({case["id"] for case in workload["cases"]}),
            "complete independent ThreadX acquisition protocols required")
    tasks = context["tasks"]
    require(isinstance(tasks, list) and 2 <= len(tasks) <= 100000,
            "at least two bounded ThreadX threads required")
    names = set()
    for task in tasks:
        fields(task, {"name", "priority", "preemption_threshold",
                      "time_slice_ticks"})
        name = identifier(task["name"], "ThreadX thread")
        require(name != "MSP" and name not in names, "repeated ThreadX thread")
        names.add(name)
        priority = _integer(task["priority"], "priority", maximum=31)
        _integer(task["preemption_threshold"], "preemption threshold",
                 maximum=priority)
        _integer(task["time_slice_ticks"], "time slice")
    require(set(context["stack_names"]) == names | {"MSP"},
            "ThreadX stacks must cover every selected thread and MSP")
    irqs = context["os_irqs"]
    require(isinstance(irqs, list) and 0 < len(irqs) <= 256 and
            len(set(irqs)) == len(irqs),
            "unique bounded ThreadX OS IRQs required")
    maximum = resolved["cpu_profile"]["irq"]["maximum_priority"]
    for name, priority in context["irq_priorities"].items():
        _integer(priority, "logical IRQ priority", maximum=maximum)
    for name in irqs:
        identifier(name, "ThreadX OS IRQ")
        require(name in context["irq_priorities"],
                "ThreadX OS IRQ not inventoried")
        if context["backend_contract"]["interrupt_mask"] == "basepri":
            require(context["irq_priorities"][name] >=
                    context["backend_contract"]["syscall_priority"],
                    "ThreadX OS IRQ exceeds selected BASEPRI ceiling")


def task_trace(trace, context):
    fields(trace, {"tasks", "switches"})
    expected = {task["name"]: task for task in context["tasks"]}
    rows = trace["tasks"]
    require(isinstance(rows, list) and len(rows) == len(expected),
            "complete ThreadX observed thread inventory required")
    seen = set()
    for row in rows:
        fields(row, {"name", "priority", "preemption_threshold",
                     "time_slice_ticks",
                     "iterations", "started_us", "finished_us"})
        name = row["name"]
        require(isinstance(name, str) and name in expected and name not in seen,
                "unreviewed/repeated observed ThreadX thread")
        seen.add(name)
        require(all(type(row[key]) is int and row[key] == value
                    for key, value in expected[name].items() if key != "name"),
                "ThreadX observed thread priority/threshold/slice mismatch")
        _integer(row["iterations"], "thread iterations",
                 context["requirements"]["task"]["minimum_iterations"])
        start = _integer(row["started_us"], "thread start")
        _integer(row["finished_us"], "thread finish", minimum=start)
    switches = trace["switches"]
    require(isinstance(switches, list) and 0 < len(switches) <= 100000,
            "bounded ThreadX dispatch observations required")
    observed = set()
    for index, row in enumerate(switches):
        fields(row, {"sequence", "expected", "observed"})
        require(type(row["sequence"]) is int and row["sequence"] == index and
                isinstance(row["expected"], str) and
                row["expected"] in expected and
                row["observed"] == row["expected"],
                "ThreadX dispatch sequence/pattern mismatch")
        observed.add(row["observed"])
    require(observed == seen, "ThreadX dispatch omits selected threads")
    return {"threads": len(rows), "dispatches": len(switches)}


def irq_trace(trace, context):
    fields(trace, {"samples"})
    samples = trace["samples"]
    require(isinstance(samples, list) and 0 < len(samples) <= 100000,
            "bounded ThreadX IRQ samples required")
    tasks = {task["name"] for task in context["tasks"]}
    stripped = []
    for row in samples:
        fields(row, {"sequence", "irq_us", "task_us", "irq", "service", "task"})
        require(isinstance(row["irq"], str) and
                row["irq"] in context["os_irqs"] and
                row["service"] == "semaphore_ceiling_put",
                "reviewed ThreadX ISR-safe service/source required")
        require(isinstance(row["task"], str) and row["task"] in tasks,
                "selected actual resumed ThreadX thread required")
        stripped.append({key: row[key] for key in
                         ("sequence", "irq_us", "task_us")})
    return {"samples": stripped}


def fail_stop_trace(trace):
    fields(trace, {"scenarios"})
    scenarios = trace["scenarios"]
    require(isinstance(scenarios, list) and len(scenarios) == 2,
            "ThreadX assert/overflow fault observations required")
    stripped = []
    for row in scenarios:
        fields(row, {"id", "trigger_us", "safe_us", "expected_output",
                     "observed_output", "observer_record", "incoming_masks",
                     "retained_loans", "terminal_progress"})
        record, masks = row["observer_record"], row["incoming_masks"]
        fields(record, {"reason", "sequence"})
        fields(masks, {"primask", "basepri", "ipsr"})
        require(record["reason"] == row["id"],
                "ThreadX fault observer reason mismatch")
        _integer(record["sequence"], "observer sequence", minimum=1)
        for name, maximum in (("primask", 1), ("basepri", 255), ("ipsr", 511)):
            _integer(masks[name], "incoming " + name, maximum=maximum)
        _integer(row["retained_loans"], "retained fault loans")
        _integer(row["terminal_progress"], "terminal progress", maximum=0)
        stripped.append({key: row[key] for key in
                         ("id", "trigger_us", "safe_us", "expected_output",
                          "observed_output")})
    return {"scenarios": stripped}

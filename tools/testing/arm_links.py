"""Run selected assembly links through the platform's ordinary firmware target."""

from concurrent.futures import ThreadPoolExecutor
from functools import partial
import signal
import threading

from tools.dev import process
from tools.dev.build_lock import build_lock
from tools.dev.presets import configure_if_needed
from tools.evidence.common import atomic_json, file_identity, load_json


def linked_image(directory, *, assembly):
    """Bind the reported image and its inputs to this assembly's build tree."""
    report = load_json(directory / "nexus_firmware.resources.json")
    elf = file_identity(directory / "nexus_firmware.elf")
    generated = directory.parent / "generated"
    resolved = generated / "resolved.json"
    budget = generated / "resource-budget.json"
    facts = load_json(resolved)
    if (report.get("schema_version") != 1 or report.get("kind") != "elf_resource_budget"
            or report.get("status") != "passed"
            or report.get("violations") != [] or report.get("elf") != elf
            or report.get("resolved") != file_identity(resolved)
            or report.get("budget") != file_identity(budget)
            or any(not isinstance(facts.get(key), str) or report.get(key) != facts[key]
                   for key in ("board", "part", "soc_family"))):
        raise ValueError(f"Resource budget or image identity mismatch: {directory}")
    authored = file_identity(assembly)
    inputs = report.get("configuration_inputs")
    if not isinstance(inputs, list) or not any(
            isinstance(item, dict) and item.get("file") == authored for item in inputs):
        raise ValueError(f"Resource report belongs to a different assembly: {assembly}")
    return {"elf": elf, "board": facts["board"], "part": facts["part"]}


def build_assembly(root, name, env, run, *, tree=None):
    """A tree is private to one authored assembly, including its generated C."""
    assembly = (root / name).resolve()
    relative = assembly.relative_to(root)
    if relative.as_posix() != name or assembly.suffix != ".toml" or not assembly.is_file():
        raise ValueError(f"Invalid selected ARM assembly: {name}")
    build = root / "build" / "affected-arm" / relative.with_suffix("")
    toolchain = str(root / "cmake/toolchains/arm-gcc.cmake")
    variables = {"CMAKE_BUILD_TYPE": "Release", "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
                 "NEXUS_BUILD_TESTS": "OFF", "NEXUS_BUILD_WORKLOAD": "ON",
                 "NEXUS_ASSEMBLY_FILE": str(assembly)}
    preset = {"binaryDir": str(build), "generator": "Ninja",
              "toolchainFile": toolchain, "cacheVariables": variables}

    def configure(options):
        run(["cmake", *options, "-S", str(root), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_TOOLCHAIN_FILE=" + toolchain,
             *(f"-D{key}={value}" for key, value in variables.items())],
            env, build / "impact-configure.txt")
        return 0

    with build_lock(build):
        configure_if_needed(root, preset, env, configure)
        run(["cmake", "--build", str(build), "--target", "nexus_firmware"],
            env, build / "impact-build.txt")
        image = linked_image(build / "bin", assembly=assembly)
        if tree is not None:
            from tools.testing.build_impact import record_hardware_inputs
            record_hardware_inputs(root, name, build, tree)
        return {"assembly": name, **image}


def affected_arm_links(root, paths, env, output, run, *, tree=None):
    """Record selection before execution so failures retain their exact scope."""
    from tools.testing.build_impact import plan_hardware

    selection = plan_hardware(root, paths)
    atomic_json(output / "arm-selection.json", selection)
    names = selection["assemblies"]
    if not names:
        return {**selection, "links": []}
    jobs = process.parallel_jobs(env)
    workers = min(jobs, len(names))
    child = process.execution_environment(env, jobs=jobs // workers)
    cancelled = threading.Event()
    command = partial(run, cancel_event=cancelled)

    def build(name):
        if cancelled.is_set():
            return None
        try:
            return build_assembly(root, name, child, command, tree=tree)
        except Exception:
            if cancelled.is_set():
                return None
            cancelled.set()
            raise

    previous = {}
    try:
        if threading.current_thread() is threading.main_thread():
            for sig in (signal.SIGINT, signal.SIGTERM):
                previous[sig] = signal.signal(sig, lambda *_: cancelled.set())
        with ThreadPoolExecutor(max_workers=workers) as pool:
            try:
                images = list(pool.map(build, names))
            except BaseException:
                cancelled.set()
                raise
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)
    if cancelled.is_set():
        raise ValueError("ARM firmware links cancelled")
    return {**selection, "links": images}

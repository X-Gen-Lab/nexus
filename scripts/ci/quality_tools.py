"""Required analyzers: findings, missing tools and zero scope all fail.

Uses actual compile commands; excludes third-party code by source ownership.
Reports preserve exact invocations and exit codes, including execution errors.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.dev import process

OWNED = {"core", "io", "os", "components", "arch", "boards", "soc", "tools"}

# Register models compile the production translation units separately with
# their actual model defines. The compilation database is the scope authority.
HOST_MODELS = {
    "tests/contracts/os_freertos_runtime/posix_event.c":
        "tests/contracts/os_freertos_runtime/posix_event.c",
    "tests/contracts/os_freertos_runtime/posix_idle.c":
        "tests/contracts/os_freertos_runtime/posix_idle.c",
}

# Required portable-C correctness profile. Advisory exclusions and their
# reviewed rationale live in docs/design/engineering-handbook.md.
TIDY_CHECKS = ",".join((
    "-*", "clang-analyzer-*", "bugprone-*", "cert-*",
    "-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling",
    "-clang-analyzer-optin.performance.Padding",
    "-bugprone-easily-swappable-parameters", "-bugprone-macro-parentheses",
    "-bugprone-reserved-identifier", "-cert-dcl37-c", "-cert-dcl51-cpp",
))

# Operand grammar is shared by compiler queries and conservative deduplication.
# Unknown syntax remains verbatim in context identity; it is never guessed away.
INCLUDE_ARGUMENT_OPTIONS = {
    "-I", "-D", "-U", "-include", "-include-pch", "-include-pth", "-imacros",
    "-iquote", "-idirafter", "-isystem",
    "-x", "-o", "-MF", "-MT", "-MQ", "-MJ", "--sysroot", "-isysroot",
    "-iprefix", "-iwithprefix", "-iwithprefixbefore", "-B", "-Xclang",
    "-Xpreprocessor", "-Xassembler", "-Xlinker", "-target", "--target",
    "-resource-dir", "--gcc-toolchain", "-F", "-iframework", "-isystem-after",
    "-arch", "--param",
}
OUTPUT_ARGUMENT_OPTIONS = {"-o", "--output", "-MF", "-MT", "-MQ", "-MJ"}
DEPENDENCY_ACTIONS = {"-MD", "-MMD", "-MP"}
SIMPLE_FLAGS = {
    "-c", "--compile", "-S", "-E", "-dM", "-M", "-MM", "-MG",
    "-fsyntax-only", "-pthread", "-pipe", "-pedantic", "-pedantic-errors",
    "-fno-common", "-fno-builtin", "-ffreestanding", "-fhosted",
    "-fdata-sections", "-ffunction-sections", "-fno-omit-frame-pointer",
    "-fomit-frame-pointer", "-fno-pie", "-fPIC", "-fPIE", "-fpic", "-fpie",
    "-fno-exceptions", "-fno-rtti", "-fstack-protector-strong",
    "-fno-stack-protector", "-fno-strict-aliasing", "-fstrict-aliasing",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
}
COMPILER_LAUNCHERS = {"ccache", "sccache", "distcc"}
COMPILER_DRIVER = r"(?:gcc|g\+\+|clang|clang\+\+|cc|c\+\+)(?:-\d+(?:\.\d+)*)?$"


def analysis_arguments(entry: dict) -> list[str]:
    """Remove known output-only fields, retaining unknown grammar verbatim."""
    argv = compilation_arguments(entry)
    compiler_index = 1 if Path(argv[0]).name in COMPILER_LAUNCHERS else 0
    if len(argv) <= compiler_index:
        raise ValueError("compiler missing behind launcher")
    if not re.search(COMPILER_DRIVER, Path(argv[compiler_index]).name):
        return argv
    # Executable names are never options, even when their spelling resembles
    # an output flag. Unknown launchers/drivers retain their complete argv.
    normal = argv[:compiler_index + 1]
    unknown = False
    index = compiler_index + 1
    while index < len(argv):
        argument = argv[index]
        if argument.startswith("@") or argument.startswith("-Wp,-M"):
            raise ValueError("response/dependency forwarding flags require explicit expansion before analysis")
        if argument == "--":
            raise ValueError("compiler end-of-options requires explicit analyzer projection")
        if argument in OUTPUT_ARGUMENT_OPTIONS:
            if index + 1 >= len(argv):
                raise ValueError("missing compiler output option value")
            index += 2
        elif (argument in DEPENDENCY_ACTIONS or argument in {"-c", "--compile"} or
              argument.startswith("--output=") or
              any(argument.startswith(option) and argument != option
                  for option in ("-MF", "-MT", "-MQ", "-MJ"))):
            index += 1
        elif argument in INCLUDE_ARGUMENT_OPTIONS:
            if index + 1 >= len(argv):
                raise ValueError("missing compiler option value: " + argument)
            normal.extend(argv[index:index + 2])
            index += 2
        else:
            normal.append(argument)
            if (argument.startswith("-") and argument not in SIMPLE_FLAGS and
                    not re.fullmatch(r"-(?:D.+|U.+|I.+|isystem[^-].*|include[^-].*|x.+|std=.+|O(?:[0-3sgz]|fast)|g(?:\d|gdb\d?)?|W.+|m(?:cpu|arch|tune|abi|float-abi|fpu)=.+|mthumb|fsanitize=.+|fno-sanitize=.+|fdiagnostics-color=.+)", argument)):
                unknown = True
            index += 1
    # An unknown option could consume a following apparent output flag. Keeping
    # the complete original argv misses only an optimization, never coverage.
    return argv if unknown else normal


def analysis_contexts(selected: list[dict]) -> list[dict]:
    """Group only exact ordered compilation semantics; preserve every member."""
    contexts = []
    identities = {}
    for index, entry in enumerate(selected):
        identity = (entry["directory"], entry["file"],
                    tuple(analysis_arguments(entry)))
        if identity not in identities:
            identities[identity] = len(contexts)
            contexts.append({"entry": entry, "compilation_indexes": []})
        contexts[identities[identity]]["compilation_indexes"].append(index)
    return contexts


def compilation_arguments(entry: dict) -> list[str]:
    """Parse one actual compilation argv without invoking a shell."""
    arguments = entry.get("arguments")
    if arguments is not None:
        if not isinstance(arguments, list) or not arguments or any(not isinstance(arg, str) or not arg or "\0" in arg for arg in arguments):
            raise ValueError("invalid compilation argv")
        argv = list(arguments)
    elif isinstance(entry.get("command"), str):
        argv = shlex.split(entry["command"], posix=True)
        if not argv or any("\0" in arg for arg in argv):
            raise ValueError("invalid compilation command")
    else:
        raise ValueError("compilation argv or command required")
    return argv


def compiler_query(entry: dict) -> tuple[list[str], list[str]]:
    """Derive a GNU/Clang preprocessor query from one actual compile argv.

    Preserve target, language, defines/undefines, include and ABI options. Remove
    only the source and compile/dependency output actions; never invoke a shell.
    Unsupported response files and launcher forms fail instead of guessing flags.
    """
    argv = compilation_arguments(entry)
    compiler_index = 1 if Path(argv[0]).name in COMPILER_LAUNCHERS else 0
    if len(argv) <= compiler_index:
        raise ValueError("compiler missing behind launcher")
    driver = Path(argv[compiler_index]).name
    if not re.search(COMPILER_DRIVER, driver):
        raise ValueError("unsupported compiler driver for predefined macro query")
    prefix = argv[:compiler_index + 1]
    remaining = []
    language = None
    source_language = None
    source_seen = False
    index = compiler_index + 1
    argument_options = OUTPUT_ARGUMENT_OPTIONS
    actions = {"-c", "--compile", "-S", "-E", "-dM", "-MD", "-MMD", "-M", "-MM", "-MG", "-MP", "-fsyntax-only"}
    while index < len(argv):
        argument = argv[index]
        if argument.startswith("@") or argument.startswith("-Wp,-M"):
            raise ValueError("response/dependency forwarding flags require explicit expansion before analysis")
        if argument in argument_options:
            if index + 1 >= len(argv):
                raise ValueError("missing compiler output option value")
            index += 2
            continue
        if (argument in actions or argument.startswith("--output=") or
                any(argument.startswith(option) and argument != option for option in ("-MF", "-MT", "-MQ", "-MJ"))):
            index += 1
            continue
        if argument.startswith("-o") and not argument.startswith(("-objc", "-object", "-openmp", "-offload")):
            index += 1
            continue
        if argument == "-x":
            if index + 1 >= len(argv):
                raise ValueError("unsupported or missing explicit compiler language")
            language = argv[index + 1]
            index += 2
            continue
        if argument.startswith("-x") and argument != "-x":
            language = argument[2:]
            index += 1
            continue
        if argument in INCLUDE_ARGUMENT_OPTIONS:
            if index + 1 >= len(argv):
                raise ValueError("missing compiler option value: " + argument)
            remaining.extend(argv[index:index + 2])
            index += 2
            continue
        if not argument.startswith("-") and (Path(entry["directory"]) / argument).resolve() == Path(entry["file"]).resolve():
            if source_seen:
                raise ValueError("multiple translation unit inputs require explicit analyzer projection")
            source_seen = True
            source_language = language
        else:
            remaining.append(argument)
        index += 1
    if not source_seen:
        raise ValueError("compilation command does not contain its translation unit")
    if source_language is None or source_language == "none":
        extension = Path(entry["file"]).suffix
        cpp = extension in {".C", ".cc", ".cpp", ".cxx", ".c++", ".CPP"} or "++" in driver
        if extension not in {".c", ".C", ".cc", ".cpp", ".cxx", ".c++", ".CPP"}:
            raise ValueError("unsupported translation unit language")
        source_language = "c++" if cpp else "c"
    remaining.extend(["-x", source_language])
    return [*prefix, "--version"], [*prefix, *remaining, "-E", "-dM", "-"]


def imported_macro(name: str) -> bool:
    # Import compiler identity, target and ABI object macros only. Project
    # definitions stay in their actual database entry, not in a global union.
    return bool(re.fullmatch(r"__(?:GNUC(?:_MINOR|_PATCHLEVEL)?|GNUG|GXX_ABI_VERSION|clang(?:_major|_minor|_patchlevel)?|llvm|VERSION|STDC(?:_VERSION|_HOSTED|_NO_ATOMICS|_NO_THREADS|_NO_VLA|_NO_COMPLEX)?|cplusplus|STRICT_ANSI|CHAR_BIT|CHAR_UNSIGNED|SIZE_TYPE|SIZE_MAX|PTRDIFF_TYPE|PTRDIFF_MAX|INTPTR_TYPE|INTPTR_MAX|UINTPTR_TYPE|UINTPTR_MAX|BYTE_ORDER)__", name) or
                re.match(r"__(?:SIZEOF_|ALIGNOF_|ORDER_|WCHAR_|WINT_|SCHAR_|SHRT_|INT(?:_|\d|_FAST|_LEAST)|UINT(?:_|\d|_FAST|_LEAST)|LONG_|LONG_LONG_|GCC_ATOMIC_|SSE|AVX|MMX|FMA|AES|PCLMUL|SHA|ARM_|arm|thumb|aarch64|x86|i[3-6]86|amd64|riscv|mips|sparc|powerpc|ppc|PPC|s390|wasm|AVR|MSP430|VFP_|SOFTFP|APCS_|linux|unix|APPLE|MACH|CYGWIN|MINGW|FreeBSD|NetBSD)", name) or
                name in {"_WIN32", "_WIN64", "__cplusplus", "__GXX_ABI_VERSION", "__LP64__", "_LP64", "__ILP32__", "_ILP32", "__LLP64__", "__OPTIMIZE__", "__OPTIMIZE_SIZE__", "__NO_INLINE__", "__FINITE_MATH_ONLY__"})


def execute(argv, *, cwd, log=None, timeout=30, input=None,
            cancel_event=None):
    """Use the maintained runner for receipts and complete child withdrawal."""
    return process.run(argv, cwd=cwd, env=os.environ.copy(), log=log,
                       timeout=timeout, input=input, cancel_event=cancel_event,
                       stage="required-static-analysis")


def captured_bytes(result, name):
    """Use original runner bytes for compiler identities and strict parsing."""
    if result.receipt_path is not None:
        return (result.receipt_path.parent / (name + ".txt")).read_bytes()
    return getattr(result, name).encode("utf-8")


def predefines(entry: dict, output, *, log_directory=None,
               cancel_event=None) -> list[str]:
    def query(argv, name, input=None):
        result = execute(argv, cwd=entry["directory"], input=input,
                         log=log_directory / name if log_directory else None,
                         cancel_event=cancel_event)
        if result.returncode == 130 and cancel_event is not None:
            cancel_event.set()
        return result

    version_argv, query_argv = compiler_query(entry)
    output.write("Compiler version invocation: " + json.dumps(version_argv) + "\n")
    output.flush()
    version = query(version_argv, "compiler-version.log", b"")
    output.write("Compiler version: " + version.stdout + version.stderr + "\n")
    if version.returncode != 0 or not version.stdout.strip():
        raise ValueError("compiler version query failed or empty")
    output.write("Compiler predefined invocation: " + json.dumps(query_argv) + "\n")
    output.flush()
    queried = query(query_argv, "compiler-predefines.log", b"")
    output.write("Compiler query exit code: " + str(queried.returncode) + "\n")
    if queried.stderr:
        output.write("Compiler query diagnostic: " + queried.stderr + "\n")
    if queried.returncode != 0 or not queried.stdout.strip():
        raise ValueError("compiler predefined query failed or empty")
    queried_bytes = captured_bytes(queried, "stdout")
    output.write("Compiler predefined output SHA256: " + hashlib.sha256(queried_bytes).hexdigest() + "\n")
    definitions = {}
    function_macros = 0
    for line in queried_bytes.decode("utf-8", errors="strict").splitlines():
        match = re.fullmatch(r"#define\s+([A-Za-z_][A-Za-z0-9_]*)(\([^\n]*?\))?(?:\s+(.*))?", line)
        if not match:
            raise ValueError("malformed compiler predefined output")
        name, parameters, value = match.groups()
        if parameters is not None:
            function_macros += 1
            continue
        if imported_macro(name):
            if name in definitions:
                raise ValueError("duplicate compiler predefined macro")
            definitions[name] = "" if value is None else value
    if not definitions or not any(name in definitions for name in ("__GNUC__", "__clang__")):
        raise ValueError("compiler identity/ABI predefined scope is empty")
    # Cppcheck does not necessarily load the compiler's system stdint.h. Query
    # the same real target/language context for its public pointer limit macros;
    # never infer their values from a host width or disable the source checks.
    output.write("Compiler stdint invocation: " + json.dumps(query_argv) + "\n")
    output.write("Compiler stdint input: " + json.dumps("#include <stdint.h>\n") + "\n")
    output.flush()
    standard = query(query_argv, "compiler-stdint.log", b"#include <stdint.h>\n")
    output.write("Compiler stdint query exit code: " + str(standard.returncode) + "\n")
    if standard.stderr:
        output.write("Compiler stdint query diagnostic: " + standard.stderr + "\n")
    if standard.returncode != 0 or not standard.stdout.strip():
        raise ValueError("compiler stdint ABI query failed or empty")
    standard_bytes = captured_bytes(standard, "stdout")
    output.write("Compiler stdint output SHA256: " + hashlib.sha256(standard_bytes).hexdigest() + "\n")
    standard_limits = {}
    for line in standard_bytes.decode("utf-8", errors="strict").splitlines():
        match = re.fullmatch(r"#define\s+(INTPTR_MAX|UINTPTR_MAX)\s+(.*)", line)
        if match:
            name, value = match.groups()
            if name in standard_limits or not value.strip():
                raise ValueError("invalid compiler stdint pointer limit")
            standard_limits[name] = value
    if set(standard_limits) != {"INTPTR_MAX", "UINTPTR_MAX"}:
        raise ValueError("compiler stdint pointer limit scope is incomplete")
    for value in standard_limits.values():
        # An alias such as __UINTPTR_MAX__ must resolve to another observed ABI
        # object macro. Undefined identifiers must not become zero in analysis.
        identifiers = re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", value)
        if any(name not in definitions and name not in standard_limits
               for name in identifiers):
            raise ValueError("compiler stdint pointer limit has an unobserved dependency")
    definitions.update(standard_limits)
    output.write("Imported object predefines: " + json.dumps(definitions, sort_keys=True) + "\n")
    output.write(f"Function-like predefines not injected: {function_macros}\n")
    output.flush()
    return ["-D" + name + ("=" + value if value else "=") for name, value in sorted(definitions.items())]


def commands(root: Path, build: Path) -> list[dict]:
    raw = json.loads((build / "compile_commands.json").read_text())
    if not isinstance(raw, list):
        raise ValueError("compilation database must be a list")
    selected = []
    for entry in raw:
        if not isinstance(entry, dict) or not isinstance(entry.get("file"), str):
            raise ValueError("invalid compilation database entry")
        directory = Path(entry.get("directory", "")).resolve()
        source = Path(entry["file"])
        source = (directory / source).resolve()
        try:
            relative = source.relative_to(root)
        except ValueError:
            continue
        model_source = HOST_MODELS.get(relative.as_posix())
        if relative.parts and (relative.parts[0] in OWNED or model_source):
            if not source.is_file():
                raise ValueError(f"missing compilation source: {source}")
            if model_source and not (root / model_source).is_file():
                raise ValueError(f"missing modeled production source: {model_source}")
            selected.append({**entry, "file": str(source), "directory": str(directory),
                             "nexus_analysis_scope": "host-model" if model_source else "production",
                             "nexus_production_source": model_source or relative.as_posix()})
    if not selected:
        raise ValueError("zero owned translation units; analysis cannot pass")
    return selected


def compilation_entry(entry: dict) -> dict:
    """Write only the JSON compilation database schema accepted by LLVM.

    Scope metadata belongs in the report. LLVM 14 rejects unknown entry fields
    and falls back to parsing without the actual compiler flags.
    """
    return {name: entry[name] for name in
            ("directory", "file", "arguments", "command", "output") if name in entry}


EXTERNAL_HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".tpp"}


def forced_include_arguments(entry: dict) -> tuple[list[str], list[str]]:
    """Separate GNU forced includes without consuming other option values."""
    argv = compilation_arguments(entry)
    normal = []
    headers = []
    index = 0
    while index < len(argv):
        argument = argv[index]
        if (argument.startswith(("-imacros", "-include-pch", "-include-pth", "-fpch-", "-Wp,")) or
                argument in {"-Xpreprocessor", "-Xclang"}):
            raise ValueError("macro-only, precompiled or forwarded preprocessing needs an explicit analyzer projection")
        if argument == "-include":
            if index + 1 >= len(argv) or not argv[index + 1]:
                raise ValueError("missing forced include file")
            headers.append(argv[index + 1])
            index += 2
        elif argument.startswith("-include"):
            headers.append(argument[len("-include"):])
            index += 1
        else:
            normal.append(argument)
            index += 1
            if argument in INCLUDE_ARGUMENT_OPTIONS and index < len(argv):
                normal.append(argv[index])
                index += 1
    return normal, headers


def observed_forced_includes(entry: dict, directory: Path, output, *,
                            log_directory=None, cancel_event=None) -> list[str]:
    """Resolve -include using its actual working directory and compiler search.

    Cppcheck 2.13 ignores both forms of -include in compile_commands.json.
    Its explicit --include option needs the same files in the same order;
    macros alone cannot replace declarations or nested header dependencies.
    Preserve symlink spelling because it can determine nested quote searches.
    """
    _, headers = forced_include_arguments(entry)
    if not headers:
        return []
    cwd = Path(entry["directory"])
    search = None
    observed = []
    for name in headers:
        candidate = cwd / name
        if candidate.with_name(candidate.name + ".gch").exists():
            raise ValueError("implicit precompiled forced include needs an explicit analyzer projection")
        if not candidate.is_file():
            if Path(name).is_absolute():
                raise ValueError("forced include file is missing: " + name)
            if search is None:
                _, query = compiler_query(entry)
                query = [argument for argument in query if argument != "-dM"]
                query.insert(-1, "-v")
                output.write("Compiler include search invocation: " + json.dumps(query) + "\n")
                output.flush()
                result = execute(query, cwd=cwd, input=b"",
                                 log=log_directory / "compiler-search.log" if log_directory else None,
                                 cancel_event=cancel_event)
                output.write("Compiler include search exit code: " + str(result.returncode) + "\n")
                output.write("Compiler include search diagnostic: " + result.stderr + "\n")
                if result.returncode == 130 and cancel_event is not None:
                    cancel_event.set()
                if result.returncode:
                    raise ValueError("compiler forced include search query failed")
                search = []
                active = False
                finished = False
                for line in captured_bytes(result, "stderr").decode("utf-8", errors="strict").splitlines():
                    if line in ('#include "..." search starts here:',
                                '#include <...> search starts here:'):
                        active = True
                    elif active and line == "End of search list.":
                        finished = True
                        break
                    elif active:
                        if not line.startswith(" ") or line.endswith(" (framework directory)"):
                            raise ValueError("unsupported compiler include search entry")
                        search.append(cwd / line.strip())
                if not finished:
                    raise ValueError("compiler include search list is incomplete")
            candidate = None
            for include_directory in search:
                found = include_directory / name
                if found.with_name(found.name + ".gch").exists():
                    raise ValueError("implicit precompiled forced include needs an explicit analyzer projection")
                if found.is_file():
                    candidate = found
                    break
            if candidate is None:
                raise ValueError("forced include was not found in observed compiler search: " + name)
        observed.append({"argument": name, "path": str(candidate.absolute()),
                         "sha256": hashlib.sha256(candidate.read_bytes()).hexdigest()})
    output.write("Observed forced includes: " + json.dumps(observed) + "\n")
    # Simplecpp 2.13 stacks multiple --include files in reverse order. One
    # wrapper with ordinary includes preserves the compiler order on every
    # invocation, including repeated headers and their declarations.
    if any(any(character in header["path"] for character in '\\"\r\n')
           for header in observed):
        raise ValueError("forced include path cannot be represented literally")
    wrapper = directory / "forced-includes.h"
    wrapper.write_text("".join('#include "' + header["path"] + '"\n'
                               for header in observed), encoding="utf-8")
    output.write("Forced include wrapper: " + json.dumps({
        "path": str(wrapper), "sha256": hashlib.sha256(wrapper.read_bytes()).hexdigest(),
        "contents": wrapper.read_text(encoding="utf-8")}) + "\n")
    return ["--include=" + str(wrapper)]


def system_include_arguments(entry: dict) -> tuple[list[str], list[str]]:
    """Split observed system includes without treating option values as flags."""
    argv = compilation_arguments(entry)
    normal = []
    systems = []
    index = 0
    while index < len(argv):
        argument = argv[index]
        if argument == "-isystem":
            if index + 1 >= len(argv):
                raise ValueError("missing system include directory")
            directory = argv[index + 1]
            index += 2
        elif argument.startswith("-isystem"):
            directory = argument[len("-isystem"):]
            index += 1
        else:
            normal.append(argument)
            index += 1
            if argument in INCLUDE_ARGUMENT_OPTIONS and index < len(argv):
                normal.append(argv[index])
                index += 1
            continue
        if not directory or directory.startswith(("=", "$SYSROOT")):
            raise ValueError("system include directory requires explicit expansion")
        systems.append(directory)
    return normal, systems


def cppcheck_compilation_entry(entry: dict) -> dict:
    """Project GNU system includes into Cppcheck's supported -I arguments.

    Cppcheck 2.13 drops -isystem in compilation databases. Keep each context's
    real headers, flags and ABI, while preserving GNU's regular-before-system
    search order. A directory named in both classes remains a system directory.
    Original compiler queries and reported source entries are never rewritten.
    Sysroot-prefixed directories require explicit expansion rather than guessing.
    """
    argv, forced = forced_include_arguments(entry)
    if forced:
        entry = {**compilation_entry(entry), "arguments": argv}
        entry.pop("command", None)
    normal, systems = system_include_arguments(entry)
    if not systems:
        return compilation_entry(entry)
    cwd = Path(entry["directory"])
    system_paths = {(cwd / path).resolve() for path in systems}
    preserved = []
    index = 0
    while index < len(normal):
        argument = normal[index]
        if argument == "-I":
            if index + 1 >= len(normal):
                raise ValueError("missing regular include directory")
            directory = normal[index + 1]
            original = normal[index:index + 2]
            index += 2
        elif argument.startswith("-I"):
            directory = argument[2:]
            original = [argument]
            index += 1
        else:
            preserved.append(argument)
            index += 1
            if argument in INCLUDE_ARGUMENT_OPTIONS and index < len(normal):
                preserved.append(normal[index])
                index += 1
            continue
        if directory == "-" or directory.startswith(("=", "$SYSROOT")):
            raise ValueError("system include priority requires explicit path expansion")
        if (cwd / directory).resolve() not in system_paths:
            preserved.extend(original)
    projected = compilation_entry(entry)
    projected.pop("command", None)
    projected["arguments"] = preserved + [arg for path in systems for arg in ("-I", path)]
    return projected


def external_system_headers(root: Path, entry: dict) -> list[dict]:
    """Enumerate exact foreign header identities in this TU's SYSTEM paths.

    This is the same ownership boundary as Clang's owned-header filter. It is
    not a diagnostic baseline: first-party headers, implementation files, paths
    not declared SYSTEM, and aliases of first-party files remain fully checked.
    """
    root = root.resolve()
    foreign = (root / "vendors", root / "ext")
    _, systems = system_include_arguments(entry)
    headers = {}
    for name in systems:
        directory = (Path(entry["directory"]) / name).resolve()
        if not directory.is_dir() or not any(directory.is_relative_to(path) for path in foreign):
            continue
        for header in sorted(directory.rglob("*")):
            canonical = header.resolve()
            if (header.suffix.lower() not in EXTERNAL_HEADER_SUFFIXES or
                    canonical.suffix.lower() not in EXTERNAL_HEADER_SUFFIXES or
                    not header.is_file() or
                    not any(canonical.is_relative_to(path) for path in foreign)):
                continue
            if any(character in str(header) + str(canonical) for character in "*?"):
                raise ValueError("external header ownership requires literal paths")
            headers[str(header)] = {
                "path": str(header), "canonical_path": str(canonical),
                "sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
                "system_include_directory": str(directory),
            }
    return [headers[name] for name in sorted(headers)]


def analyze_context(kind, root, tool, index, context, directory, cancel_event,
                    timeout):
    """Analyze one actual context with isolated files and retained commands."""
    output = io.StringIO()
    entry = context["entry"]
    try:
        directory.mkdir()
        database = directory / "compile_commands.json"
        if kind == "tidy":
            database.write_text(json.dumps([compilation_entry(entry)]))
            argv = [tool, entry["file"], "-p", str(directory),
                    "--checks=" + TIDY_CHECKS, "--warnings-as-errors=*",
                    "--header-filter=" + str(root) +
                    "/(core|io|os|components|arch|boards|soc|tools)/.*"]
        else:
            projected = cppcheck_compilation_entry(entry)
            database.write_text(json.dumps([projected]))
            output.write(f"Predefined source entry {index}: " + json.dumps(entry) + "\n")
            output.write(f"Cppcheck source projection {index}: " + json.dumps(projected) + "\n")
            observed = predefines(entry, output, log_directory=directory,
                                  cancel_event=cancel_event)
            forced = observed_forced_includes(
                entry, directory, output, log_directory=directory,
                cancel_event=cancel_event)
            external = external_system_headers(root, entry)
            output.write("External system header exclusions: " + json.dumps(external) + "\n")
            output.write("External header ownership rule: exact header files in this configuration's SYSTEM paths under vendors/ or ext/ only; owned headers and implementation files remain checked.\n")
            # Cppcheck 2.13 reports project headers relative to its working
            # directory even when the compilation entry uses absolute paths.
            # Match only exact aliases of the same reviewed foreign headers;
            # basename globs could hide an unrelated first-party header.
            exclusions = sorted({alias for header in external for path in
                                 (header["path"], header["canonical_path"])
                                 for alias in (path, str(Path(path).relative_to(root)))})
            argv = [tool, "--project=" + str(database), *observed, *forced,
                    "--enable=warning,performance,portability",
                    "--check-level=exhaustive", "--error-exitcode=2", "--xml",
                    "--xml-version=2", "--suppress=missingIncludeSystem",
                    *("--suppress=*:" + path for path in exclusions)]
        output.write("Invocation: " + json.dumps(argv) + "\n")
        result = execute(argv, cwd=root, log=directory / "analysis.log",
                         timeout=timeout, cancel_event=cancel_event)
        output.write(result.stdout + result.stderr)
        output.write(f"Exit code: {result.returncode}\n")
        output.write("Command receipt: " + str(result.receipt_path) + "\n")
        if result.returncode == 130:
            cancel_event.set()
        return result.returncode, output.getvalue()
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        output.write(f"Analysis execution failed: {error}\n")
        return 1, output.getvalue()


def run(kind: str, root: Path, build: Path, tool: str, report: Path, *,
        parallel=1, timeout=180) -> int:
    report.parent.mkdir(parents=True, exist_ok=True)
    cancel_event = threading.Event()
    previous = {}
    try:
        parallel = process.positive_integer(parallel, "analysis parallelism")
        timeout = float(timeout)
        if not 0 < timeout < float("inf"):
            raise ValueError("analysis timeout must be finite and positive")
        root = root.resolve()
        selected = commands(root, build.resolve())
        contexts = analysis_contexts(selected)
        attempts = report.parent / (report.stem + ".contexts")
        attempts.mkdir(exist_ok=True)
        directory = Path(tempfile.mkdtemp(prefix="run-", dir=attempts))
        with report.open("w", encoding="utf-8") as output:
            output.write(f"Owned translation units: {len(selected)}\n")
            output.write(f"Unique analysis contexts: {len(contexts)}\n")
            output.write(f"Analysis parallelism: {parallel}\n")
            output.write("Scope: actual compilation database; host models do not establish ARM execution or hardware validation.\n")
            for index, entry in enumerate(selected):
                output.write("Source scope: " + json.dumps({
                    "compilation_index": index,
                    "translation_unit": entry["file"],
                    "kind": entry["nexus_analysis_scope"],
                    "production_source": entry["nexus_production_source"],
                    "compilation_entry": compilation_entry(entry),
                }) + "\n")
            for index, context in enumerate(contexts):
                output.write("Analysis context: " + json.dumps({
                    "context_index": index,
                    "compilation_indexes": context["compilation_indexes"],
                    "semantic_arguments": analysis_arguments(context["entry"]),
                }) + "\n")
            output.flush()
            version = execute([tool, "--version"], cwd=root,
                              log=directory / "version.log", input=b"",
                              cancel_event=cancel_event)
            output.write(version.stdout + version.stderr)
            if version.returncode != 0:
                if version.returncode == 127:
                    output.write("Analysis execution failed: analyzer could not be launched\n")
                output.write(f"Version command failed: {version.returncode}\n")
                return 130 if version.returncode == 130 else 1
            if threading.current_thread() is threading.main_thread():
                for signum in (signal.SIGINT, signal.SIGTERM):
                    previous[signum] = signal.getsignal(signum)
                    signal.signal(signum, lambda signum, frame: cancel_event.set())
            # Workers never share a database, output stream, forced-header
            # wrapper or runner log. Wait for every retained result even after
            # one finding; cancellation withdraws active children and prevents
            # queued commands from launching through the shared runner event.
            failed = False
            with concurrent.futures.ThreadPoolExecutor(max_workers=parallel) as pool:
                futures = [pool.submit(
                    analyze_context, kind, root, tool, index, context,
                    directory / str(index), cancel_event, timeout)
                    for index, context in enumerate(contexts)]
                for future in futures:
                    code, text = future.result()
                    output.write(text)
                    output.flush()
                    failed |= code != 0
            return 130 if cancel_event.is_set() else int(failed)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        with report.open("a", encoding="utf-8") as output:
            output.write(f"Analysis execution failed: {error}\n")
        return 1
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("tidy", "cppcheck"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--parallel", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()
    return run(args.kind, args.root, args.build_dir, args.tool, args.report,
               parallel=args.parallel, timeout=args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())

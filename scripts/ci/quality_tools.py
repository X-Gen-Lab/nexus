"""Required analyzers: findings, missing tools and zero scope all fail.

Uses actual compile commands; excludes third-party code by source ownership.
Reports preserve exact invocations and exit codes, including execution errors.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

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
    compiler_index = 1 if Path(argv[0]).name in {"ccache", "sccache", "distcc"} else 0
    if len(argv) <= compiler_index:
        raise ValueError("compiler missing behind launcher")
    driver = Path(argv[compiler_index]).name
    if not re.search(r"(?:gcc|g\+\+|clang|clang\+\+|cc|c\+\+)(?:-\d+(?:\.\d+)*)?$", driver):
        raise ValueError("unsupported compiler driver for predefined macro query")
    prefix = argv[:compiler_index + 1]
    remaining = []
    explicit_language = False
    source_seen = False
    index = compiler_index + 1
    argument_options = {"-o", "--output", "-MF", "-MT", "-MQ", "-MJ"}
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
        if argument.startswith("-o") and not argument.startswith(("-objc", "-object", "-openmp")):
            index += 1
            continue
        if argument == "-x":
            if index + 1 >= len(argv) or argv[index + 1] == "none":
                raise ValueError("unsupported or missing explicit compiler language")
            explicit_language = True
            remaining.extend(argv[index:index + 2])
            index += 2
            continue
        if argument.startswith("-x") and argument != "-x":
            explicit_language = True
        if not argument.startswith("-") and (Path(entry["directory"]) / argument).resolve() == Path(entry["file"]).resolve():
            source_seen = True
        else:
            remaining.append(argument)
        index += 1
    if not source_seen:
        raise ValueError("compilation command does not contain its translation unit")
    if not explicit_language:
        extension = Path(entry["file"]).suffix
        cpp = extension in {".C", ".cc", ".cpp", ".cxx", ".c++", ".CPP"} or "++" in driver
        if extension not in {".c", ".C", ".cc", ".cpp", ".cxx", ".c++", ".CPP"}:
            raise ValueError("unsupported translation unit language")
        remaining.extend(["-x", "c++" if cpp else "c"])
    return [*prefix, "--version"], [*prefix, *remaining, "-E", "-dM", "-"]


def imported_macro(name: str) -> bool:
    # Import compiler identity, target and ABI object macros only. Project
    # definitions stay in their actual database entry, not in a global union.
    return bool(re.fullmatch(r"__(?:GNUC(?:_MINOR|_PATCHLEVEL)?|GNUG|GXX_ABI_VERSION|clang(?:_major|_minor|_patchlevel)?|llvm|VERSION|STDC(?:_VERSION|_HOSTED|_NO_ATOMICS|_NO_THREADS|_NO_VLA|_NO_COMPLEX)?|cplusplus|STRICT_ANSI|CHAR_BIT|CHAR_UNSIGNED|SIZE_TYPE|SIZE_MAX|PTRDIFF_TYPE|PTRDIFF_MAX|INTPTR_TYPE|INTPTR_MAX|UINTPTR_TYPE|UINTPTR_MAX|BYTE_ORDER)__", name) or
                re.match(r"__(?:SIZEOF_|ALIGNOF_|ORDER_|WCHAR_|WINT_|SCHAR_|SHRT_|INT(?:_|\d|_FAST|_LEAST)|UINT(?:_|\d|_FAST|_LEAST)|LONG_|LONG_LONG_|GCC_ATOMIC_|SSE|AVX|MMX|FMA|AES|PCLMUL|SHA|ARM_|arm|thumb|aarch64|x86|i[3-6]86|amd64|riscv|mips|sparc|powerpc|ppc|PPC|s390|wasm|AVR|MSP430|VFP_|SOFTFP|APCS_|linux|unix|APPLE|MACH|CYGWIN|MINGW|FreeBSD|NetBSD)", name) or
                name in {"_WIN32", "_WIN64", "__cplusplus", "__GXX_ABI_VERSION", "__LP64__", "_LP64", "__ILP32__", "_ILP32", "__LLP64__", "__OPTIMIZE__", "__OPTIMIZE_SIZE__", "__NO_INLINE__", "__FINITE_MATH_ONLY__"})


def predefines(entry: dict, output) -> list[str]:
    version_argv, query_argv = compiler_query(entry)
    output.write("Compiler version invocation: " + json.dumps(version_argv) + "\n")
    output.flush()
    version = subprocess.run(version_argv, cwd=entry["directory"], stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
    output.write("Compiler version: " + version.stdout.decode("utf-8", errors="replace") + "\n")
    if version.returncode != 0 or not version.stdout.strip():
        raise ValueError("compiler version query failed or empty")
    output.write("Compiler predefined invocation: " + json.dumps(query_argv) + "\n")
    output.flush()
    queried = subprocess.run(query_argv, cwd=entry["directory"], input=b"",
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    output.write("Compiler query exit code: " + str(queried.returncode) + "\n")
    if queried.stderr:
        output.write("Compiler query diagnostic: " + queried.stderr.decode("utf-8", errors="replace") + "\n")
    if queried.returncode != 0 or not queried.stdout.strip():
        raise ValueError("compiler predefined query failed or empty")
    output.write("Compiler predefined output SHA256: " + hashlib.sha256(queried.stdout).hexdigest() + "\n")
    definitions = {}
    function_macros = 0
    for line in queried.stdout.decode("utf-8", errors="strict").splitlines():
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
    standard = subprocess.run(query_argv, cwd=entry["directory"],
                              input=b"#include <stdint.h>\n",
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=30)
    output.write("Compiler stdint query exit code: " + str(standard.returncode) + "\n")
    if standard.stderr:
        output.write("Compiler stdint query diagnostic: " + standard.stderr.decode("utf-8", errors="replace") + "\n")
    if standard.returncode != 0 or not standard.stdout.strip():
        raise ValueError("compiler stdint ABI query failed or empty")
    output.write("Compiler stdint output SHA256: " + hashlib.sha256(standard.stdout).hexdigest() + "\n")
    standard_limits = {}
    for line in standard.stdout.decode("utf-8", errors="strict").splitlines():
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


INCLUDE_ARGUMENT_OPTIONS = {
    "-I", "-D", "-U", "-include", "-imacros", "-iquote", "-idirafter",
    "-x", "-o", "-MF", "-MT", "-MQ", "-MJ", "--sysroot", "-isysroot",
    "-iprefix", "-iwithprefix", "-iwithprefixbefore", "-B", "-Xclang",
    "-Xpreprocessor", "-Xassembler", "-Xlinker", "-target", "--target",
}
EXTERNAL_HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".tpp"}


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


def run(kind: str, root: Path, build: Path, tool: str, report: Path) -> int:
    report.parent.mkdir(parents=True, exist_ok=True)
    try:
        selected = commands(root.resolve(), build.resolve())
        with tempfile.TemporaryDirectory(prefix="nexus-analysis-") as temp:
            (Path(temp) / "compile_commands.json").write_text(
                json.dumps([compilation_entry(entry) for entry in selected]))
            if kind == "tidy":
                invocations = []
                for index, entry in enumerate(selected):
                    # One database per invocation: repeated source files can
                    # have different provider/config definitions. An analyzer
                    # must not silently reuse the first command for every one.
                    directory = Path(temp) / str(index)
                    directory.mkdir()
                    (directory / "compile_commands.json").write_text(json.dumps([compilation_entry(entry)]))
                    invocations.append([tool, entry["file"], "-p", str(directory),
                     "--checks=" + TIDY_CHECKS,
                     "--warnings-as-errors=*",
                     "--header-filter=" + str(root.resolve()) + "/(core|io|os|components|arch|boards|soc|tools)/.*"])
            else:
                invocations = []
            failed = False
            with report.open("w", encoding="utf-8") as output:
                output.write(f"Owned translation units: {len(selected)}\n")
                output.write("Scope: actual compilation database; host models do not establish ARM execution or hardware validation.\n")
                for entry in selected:
                    output.write("Source scope: " + json.dumps({
                        "translation_unit": entry["file"],
                        "kind": entry["nexus_analysis_scope"],
                        "production_source": entry["nexus_production_source"],
                    }) + "\n")
                output.flush()
                version = subprocess.run([tool, "--version"], cwd=root,
                                         stdout=output, stderr=subprocess.STDOUT,
                                         timeout=30)
                if version.returncode != 0:
                    output.write(f"Version command failed: {version.returncode}\n")
                    return 1
                if kind == "cppcheck":
                    for index, entry in enumerate(selected):
                        # Each configuration keeps its compiler's observed
                        # predefines. Different targets of the same source
                        # are never merged into one macro environment.
                        directory = Path(temp) / str(index)
                        directory.mkdir()
                        projected = cppcheck_compilation_entry(entry)
                        (directory / "compile_commands.json").write_text(json.dumps([projected]))
                        output.write(f"Predefined source entry {index}: " + json.dumps(entry) + "\n")
                        output.write(f"Cppcheck source projection {index}: " + json.dumps(projected) + "\n")
                        observed = predefines(entry, output)
                        external = external_system_headers(root, entry)
                        output.write("External system header exclusions: " + json.dumps(external) + "\n")
                        output.write("External header ownership rule: exact header files in this configuration's SYSTEM paths under vendors/ or ext/ only; owned headers and implementation files remain checked.\n")
                        exclusions = sorted({path for header in external for path in
                                             (header["path"], header["canonical_path"])})
                        invocations.append([tool, "--project=" + str(directory / "compile_commands.json"),
                                            *observed, "--enable=warning,performance,portability",
                                            "--check-level=exhaustive",
                                            "--error-exitcode=2", "--xml", "--xml-version=2",
                                            "--suppress=missingIncludeSystem",
                                            *("--suppress=*:" + path for path in exclusions)])
                for argv in invocations:
                    output.write("Invocation: " + json.dumps(argv) + "\n")
                    output.flush()
                    result = subprocess.run(argv, cwd=root, stdout=output,
                                            stderr=subprocess.STDOUT, timeout=180)
                    output.write(f"Exit code: {result.returncode}\n")
                    failed |= result.returncode != 0
            return 1 if failed else 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        with report.open("a", encoding="utf-8") as output:
            output.write(f"Analysis execution failed: {error}\n")
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("tidy", "cppcheck"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    return run(args.kind, args.root, args.build_dir, args.tool, args.report)


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Verify independent consumers of an existing publishable source SDK prefix."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def clean_environment():
    environment = os.environ.copy()
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "LIBRARY_PATH"):
        environment.pop(name, None)
    return environment


def prepare_consumer(prefix, directory, name, run_checked, *, fixture=False,
                     arm=False, chip="stm32", expected_revision=None,
                     version="1.0.0", contracts=False):
    """One public consumer template shared by qualification and boundary tests."""
    prefix = Path(prefix)
    sdk = prefix / "share/nexus/src"
    source = directory / (name + " source")
    build = directory / (name + " build")
    source.mkdir()
    # A parent Git identity must never become the SDK source identity.
    run_checked(["git", "init", "--quiet", str(source)])
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.31)\n'
        'project(installed_source_consumer LANGUAGES C CXX ASM)\n'
        'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
        f'find_package(Nexus {version} EXACT CONFIG REQUIRED)\n'
        'find_package(Nexus CONFIG REQUIRED)\n'
        'add_executable(parent_plain parent.c)\n'
        'nexus_add_firmware(installed_c SOURCES main.c)\n'
        'nexus_add_firmware(installed_cpp SOURCES main.cpp LIBRARIES Nexus::Log Nexus::BMP280 Nexus::Storage)\n'
        'target_compile_options(installed_cpp PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")\n'
        'get_target_property(sdk_root Nexus::Config NEXUS_SOURCE_DIR)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/resolved-sdk.txt" "${sdk_root}")\n')
    (source / "parent.c").write_text(
        '#ifdef NEXUS_EFFECTIVE_CONFIG\n#error SDK leaked into unrelated parent target\n#endif\n'
        'int main(void) { return 0; }\n')
    code = ('#include "nexus_bindings.h"\n'
            '#include "nexus_factory.h"\n'
            '#include "nexus_config.h"\n'
            '#if __has_include("stm32f4xx.h") || __has_include("gd32f4xx.h")\n'
            '#error Vendor SDK leaked into product source\n#endif\n'
            '#if __has_include("stm32f407_provider.h") || __has_include("gd32f470_provider.h")\n'
            '#error Concrete provider leaked into product source\n#endif\n'
            'int main(void) {\n'
            ' if (nx_factory_gpio(NX_GPIO_ID_COUNT) != 0) return 7;\n'
            ' nx_platform_start_result_t result = nx_platform_start();\n'
            ' if (result.primary != NX_SUCCESS) return 1;\n'
            ' return nx_platform_stop() == NX_SUCCESS ? 0 : 2;\n}\n')
    (source / "main.c").write_text(code)
    (source / "main.cpp").write_text(
        '#include "nexus/components/log.h"\n'
        '#include "nexus/components/bmp280.h"\n'
        '#include "nexus/components/storage.h"\n'
        '#include <cstring>\n' + code.replace(
            ' nx_platform_start_result_t result',
            ' nx_log_sink_port_t sink;\n std::memset(&sink, 0, sizeof(sink));\n'
            ' if (nx_log_init(nullptr, sink, NX_LOG_INFO) != NX_ERROR_INVALID) return 3;\n'
            ' if (nx_bmp280_compensate(nullptr, 0, 0, nullptr) != NX_ERROR_INVALID) return 4;\n'
            ' nx_platform_start_result_t result'))
    arguments = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DNexus_DIR={prefix / 'lib/cmake/Nexus'}",
                 "-DNEXUS_BUILD_TESTS=OFF", "-DNEXUS_BUILD_WORKLOAD=OFF"]
    if fixture:
        arguments.append("-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON")
    if contracts:
        arguments.append("-DNEXUS_BUILD_WORKLOAD=ON")
    if expected_revision:
        arguments.append("-DNEXUS_EXPECTED_SOURCE_REVISION=" + expected_revision)
    arm_assembly = "liangshan-baremetal.toml" if chip == "gd32" else "sky-baremetal.toml"
    assembly = sdk / "tools/configure/assemblies" / (arm_assembly if arm else "native.toml")
    arguments.append(f"-DNEXUS_ASSEMBLY_FILE={assembly}")
    if arm:
        arguments.append(f"-DCMAKE_TOOLCHAIN_FILE={sdk / 'cmake/toolchains/arm-gcc.cmake'}")
    return source, build, arguments


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    prefix, output = args.prefix.absolute(), args.output.absolute()
    records = []
    report = {"schema_version": 1, "kind": "source_sdk_consumer_verification",
              "prefix": str(prefix), "status": "failed",
              "physical_status": "not_executed", "commands": records}
    try:
        if prefix.resolve() != prefix or output.resolve() != output or output.is_relative_to(prefix):
            raise ValueError("Consumer output must be canonical and outside the SDK")
        if output.exists():
            if not (output / ".nexus-sdk-consumer-check").is_file():
                raise ValueError("Existing output is not an owned consumer verification bundle")
            shutil.rmtree(output)
        output.mkdir(parents=True)
        (output / ".nexus-sdk-consumer-check").write_text("owned SDK consumer verification\n")
        def run_checked(command):
            result = subprocess.run(command, capture_output=True, text=True,
                                    env=clean_environment(), timeout=180)
            index = len(records)
            log = output / f"command-{index:02d}.log"
            log.write_text(result.stdout + result.stderr)
            records.append({"argv": [str(value) for value in command],
                            "exit_code": result.returncode, "log": log.name})
            if result.returncode:
                raise ValueError(f"Consumer command {index} failed; see {log}")
            return result
        sdk = prefix / "share/nexus/src"
        run_checked([sys.executable, "-B", str(sdk / "cmake/package/package_source_sdk.py"),
                     "--verify", str(sdk)])
        identity = json.loads((sdk / ".nexus-source-sdk.json").read_text())
        if identity["publishable"] is not True or identity["source_dirty"]:
            raise ValueError("Formal consumer verification requires a publishable committed source SDK")
        report["source_revision"] = identity["source_revision"]
        report["source_tree"] = identity["source_tree"]
        for name, arm, chip in (("native", False, "native"),
                                ("stm32", True, "stm32"), ("gd32", True, "gd32")):
            source, build, command = prepare_consumer(prefix, output, name,
                run_checked, arm=arm, chip=chip, expected_revision=identity["source_revision"])
            run_checked(command)
            run_checked(["cmake", "--build", str(build), "--parallel", "4",
                         "--target", "installed_c", "installed_cpp"])
            commands = json.loads((build / "compile_commands.json").read_text())
            for entry in commands:
                if Path(entry["file"]).name not in {"main.c", "main.cpp"}:
                    continue
                if any(value in entry["command"] for value in
                       ("/vendors/", "/soc/", "-DSTM32F407", "-DGD32F470")):
                    raise ValueError("Private implementation input leaked into product source")
            if arm:
                for application in ("installed_c", "installed_cpp"):
                    resources = json.loads((build / "bin" / (application + ".resources.json")).read_text())
                    if resources["status"] != "passed":
                        raise ValueError("MCU consumer resource/configuration binding failed")
            else:
                for application in ("installed_c", "installed_cpp"):
                    run_checked([str(build / "bin" / application)])
        report["status"] = "passed"
    except (OSError, ValueError, subprocess.TimeoutExpired, KeyError) as error:
        report["error"] = str(error)
        print(f"Source SDK consumers rejected: {error}", file=sys.stderr)
    if output.is_dir() and (output / ".nexus-sdk-consumer-check").is_file():
        (output / "verification.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

"""TLS carries its entropy API when no hardware source is selected."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.testing import test_optional_io_domains as optional_io


ROOT = optional_io.ROOT


TLS_SOURCE = r'''
#include "nexus/tls/storage.h"
#include <stddef.h>
typedef struct {
    nx_time_us_t now;
    uint32_t state;
    unsigned words;
} source_t;
static nx_time_us_t clock_read(void* context) {
    source_t* source = context;
    return source->now++;
}
static nx_result_t entropy_read(void* context, uint32_t* word) {
    source_t* source = context;
    /* Insecure deterministic fixture; never a product entropy fallback. */
    source->state = source->state * 1664525U + 1013904223U;
    source->words++;
    *word = source->state;
    return NX_SUCCESS;
}
static nx_result_t utc_read(void* context, int64_t* seconds) {
    (void)context;
    *seconds = INT64_C(1800000000);
    return NX_SUCCESS;
}
int main(void) {
    _Alignas(max_align_t) static unsigned char arena[NX_TLS_ARENA_BYTES];
    nx_tls_runtime_t runtime = {0};
    source_t source = {1U, 1U, 0U};
    const nx_entropy_ops_t ops = {entropy_read};
    const nx_entropy_port_t entropy = {&ops, &source};
    const nx_tls_runtime_config_t config = {
        .arena = arena, .arena_bytes = sizeof(arena), .entropy = &entropy,
        .clock = {clock_read, &source}, .utc = {utc_read, &source},
        .entropy_timeout_us = 5000U,
        .entropy_quality = NX_TLS_ENTROPY_FULL_SOURCE};
    if (nx_tls_runtime_start(&runtime, &config, 100000U) != NX_SUCCESS ||
        source.words == 0U || !runtime.initialized)
        return 1;
    return nx_tls_runtime_stop(&runtime) != NX_SUCCESS || runtime.initialized;
}
'''


class TLSIODomainTests(unittest.TestCase):
    def prepare(self, root):
        """Use real TLS/vendor targets over the physical GPIO-only I/O graph."""
        optional_io.OptionalIODomainTests.prepare(self, root, "probe")
        path = root / "CMakeLists.txt"
        project = path.read_text().replace(
            "add_executable(consumer main.c)", f'''
add_library(nexus_config INTERFACE)
add_library(Nexus::Config ALIAS nexus_config)
set_property(TARGET nexus_config PROPERTY NEXUS_SOURCE_DIR "{ROOT}")
set_property(TARGET nexus_config PROPERTY NEXUS_LTO_REQUIRED FALSE)
include("{ROOT / 'cmake/platform/TargetPolicy.cmake'}")
include("{ROOT / 'components/tls/CMakeLists.txt'}")
nexus_add_tls()
add_executable(consumer main.c)
''').replace("Nexus::IOCycleProbe", "Nexus::TLSStorage")
        path.write_text(project)
        (root / "main.c").write_text(TLS_SOURCE)

    def build_and_run(self, root):
        environment = dict(os.environ)
        environment["PATH"] = (
            str(ROOT / ".venv/bin") + os.pathsep + environment.get("PATH", ""))
        cmake = shutil.which("cmake", path=environment["PATH"])
        self.assertIsNotNone(cmake, "Prepared CMake is required")
        commands = [
            ("configure", [cmake, "-S", str(root), "-B", str(root / "build"),
                           "-G", "Ninja"]),
            ("build", [cmake, "--build", str(root / "build"), "--target",
                       "consumer", "--parallel", "2"]),
            ("run", [str(root / "build/consumer")]),
        ]
        for stage, command in commands:
            result = subprocess.run(command, env=environment,
                                    capture_output=True, text=True, timeout=180)
            output = result.stdout + result.stderr
            (root / f"{stage}.log").write_text(output)
            (root / f"{stage}.command.json").write_text(json.dumps({
                "command": command, "exit_code": result.returncode,
            }, indent=2) + "\n")
            self.assertEqual(result.returncode, 0, output)
        entries = json.loads((root / "build/compile_commands.json").read_text())
        paths = [Path(entry["file"]).resolve() for entry in entries]
        for relative in ("components/tls/src/tls.c", "io/src/api/entropy.c",
                         "ext/mbedtls/library/ssl_tls.c",
                         "ext/mbedtls/tf-psa-crypto/core/psa_crypto.c"):
            self.assertEqual(paths.count(ROOT / relative), 1, relative)
        self.assertFalse(any((ROOT / "soc/native") in path.parents
                             for path in paths))
        self.assertNotIn("--gc-sections", (root / "build/build.ninja").read_text())

    def test_tls_uses_caller_entropy_without_a_selected_entropy_controller(self):
        with tempfile.TemporaryDirectory(prefix="nexus TLS IO ") as folder:
            root = Path(folder)
            self.prepare(root)
            self.build_and_run(root)


if __name__ == "__main__":
    unittest.main()

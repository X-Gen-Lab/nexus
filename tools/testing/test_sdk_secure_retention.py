"""The exact RNG secure owner proves typed faces, code and static storage."""
from pathlib import Path
from types import SimpleNamespace
import unittest
import tempfile
import shutil
import struct
from unittest.mock import patch

from tools.evidence.common import EvidenceError
from tools.measurement import retention

ROOT = Path(__file__).resolve().parents[2]
FUNCTIONS = (
    "nx_entropy_fill", "nx_tls_runtime_start", "nx_tls_runtime_stop",
    "nx_tls_initialize", "nx_tls_handshake_step", "nx_tls_abort",
    "nx_mqtt_init", "nx_mqtt_deinit", "MQTT_Init", "psa_crypto_init",
    "mbedtls_ssl_handshake_step", "nx_platform_start", "nx_platform_stop",
    "lwip_init", "nx_lwip_adapter_start", "nx_lwip_adapter_service",
    "nx_lwip_adapter_stop", "nx_factory_entropy",
    "nx_stm32f407_entropy_admit", "nx_stm32f407_entropy_start", "nx_stm32f407_entropy_stop",
    "nx_mqtt_connect", "nx_mqtt_publish", "nx_mqtt_process", "nx_mqtt_disconnect")


BINDINGS = """
static nx_stm32f407_entropy_state_t s_nx_port_random;
static const nx_entropy_port_t s_nx_face_random = {
    &nx_stm32f407_entropy_ops, &s_nx_port_random
};
"""
CONTROLLERS = [dict(kind="entropy", id="random", controller="RNG",
                    binding="rng", route="rng-internal", pins=[],
                    mode="rng-words", initial=0, mask=0)]


def image():
    # External ELF bytes/metadata model; actual SDK ELF execution is separate.
    objects = [("tls_arena", 65536), ("tls_stage", 512), ("mqtt_network", 1024),
               ("ram_heap", 16384), ("memp_memory_PBUF_POOL_base", 12288),
               ("memp_memory_TCP_PCB_base", 256), ("memp_memory_TCP_SEG_base", 256)]
    symbols = [{"name": name, "value": 0x08000001 + i * 16, "size": 16,
                "section": 0, "type": 2, "binding": 1}
               for i, name in enumerate(FUNCTIONS)]
    address = 0x20000000
    for name, size in objects:
        symbols.append({"name": name, "value": address, "size": size,
                        "section": 1, "type": 1, "binding": 0})
        address += size
    symbols.extend([
        {"name": "try_read", "value": 0x08000801, "size": 16,
         "section": 0, "type": 2, "binding": 0, "file": "entropy.c"},
        {"name": "s_nx_face_random", "value": 0x08010000, "size": 8,
         "section": 2, "type": 1, "binding": 0},
        {"name": "nx_stm32f407_entropy_ops", "value": 0x08010008, "size": 4,
         "section": 2, "type": 1, "binding": 1},
        {"name": "s_nx_port_random", "value": 0x2001f000, "size": 24,
         "section": 1, "type": 1, "binding": 0}])
    data = bytearray(4352)
    struct.pack_into("<3I", data, 4096,
                     0x08010008, 0x2001f000, 0x08000801)
    return SimpleNamespace(data=data, sections=[
        {"name": ".text", "flags": 6, "address": 0x08000000, "offset": 0,
         "size": 4096, "kind": 1},
        {"name": ".bss", "flags": 3, "address": 0x20000000, "offset": 4096,
         "size": 131072, "kind": 8},
        {"name": ".rodata", "flags": 2, "address": 0x08010000,
         "offset": 4096, "size": 256, "kind": 1}], loads=[
        {"offset": 0, "virtual": 0x08000000, "physical": 0x08000000,
         "files": 4096, "memory": 4096, "flags": 5},
        {"offset": 4096, "virtual": 0x20000000, "physical": 0x20000000,
         "files": 0, "memory": 131072, "flags": 6},
        {"offset": 4096, "virtual": 0x08010000, "physical": 0x08010000,
         "files": 256, "memory": 256, "flags": 4}], defined_symbols=symbols)


class SDKSecureRetentionTests(unittest.TestCase):
    def inspect(self, elf=None, *, source_mutation=False, **changes):
        resolved = dict(soc_family="stm32f4", part="STM32F407ZGT6",
                        backend="baremetal", clock_profile="hse8-pll168",
                        controllers=CONTROLLERS, devices=[])
        resolved.update(changes)
        with tempfile.TemporaryDirectory() as directory:
            copied = Path(directory)
            canonical = ROOT / "cmake/package/verification/secure_services"
            for path in (canonical / "f407").iterdir():
                shutil.copyfile(path, copied / path.name)
            shutil.copyfile(canonical / "public_ca.h", copied / "public_ca.h")
            if source_mutation:
                path = copied / "application.c"
                path.write_text(path.read_text() + "\n/* changed */\n")
            return retention.inspect_provider_retention(
                elf or image(), BINDINGS, root=ROOT, resolved=resolved,
                component_owner="f407-secure-services",
                consumer_path=copied / "application.c")

    def test_exact_secure_owner_keeps_code_and_real_static_pools(self):
        report = self.inspect()
        self.assertEqual(report["kind"], "linked_component_owner_retention")
        self.assertEqual(report["owner_profile"], "f407-secure-services")
        self.assertEqual(report["face_count"], 1)
        self.assertEqual(report["operations_table_count"], 1)
        self.assertEqual(report["faces"][0]["operations"],
                         "nx_stm32f407_entropy_ops")
        self.assertEqual(set(item["name"] for item in report["entrypoints"]),
                         set(FUNCTIONS))
        self.assertEqual(report["storage"]["tls_arena"]["size"], 65536)

    def test_wrong_domain_and_default_empty_owner_still_fail(self):
        with self.assertRaisesRegex(EvidenceError, "no selected faces"):
            retention.inspect_provider_retention(image(), "")
        for change in ({"part": "STM32F407VET6"}, {"backend": "freertos"},
                       {"controllers": []}, {"controllers": [{"kind": "gpio"}]}):
            with self.subTest(change=change), self.assertRaises(EvidenceError):
                self.inspect(**change)

    def test_rng_face_wiring_and_direct_method_are_not_optional(self):
        for offset in (4096, 4100, 4104):
            with self.subTest(offset=offset), self.assertRaises(EvidenceError):
                elf = image()
                struct.pack_into("<I", elf.data, offset, 0)
                self.inspect(elf)

    def test_nonfunction_and_weak_entries_and_wrong_storage_fail(self):
        for fault in ("type", "weak", "storage", "executable_ram"):
            with self.subTest(fault=fault), self.assertRaises(EvidenceError):
                elf = image()
                if fault == "type":
                    elf.defined_symbols[0]["type"] = 1
                elif fault == "weak":
                    elf.defined_symbols[0]["binding"] = 2
                elif fault == "storage":
                    next(s for s in elf.defined_symbols
                         if s["name"] == "tls_arena")["size"] -= 8
                else:
                    elf.sections[1]["flags"] |= 4
                self.inspect(elf)

    def test_changed_caller_and_libc_heap_cpp_bodies_fail(self):
        with self.assertRaisesRegex(EvidenceError, "canonical source"):
            self.inspect(source_mutation=True)
        for name in ("malloc", "_sbrk_r", "__cxa_guard_acquire", "_Znwm"):
            with self.subTest(name=name), self.assertRaisesRegex(
                    EvidenceError, r"heap|C\+\+"):
                elf = image()
                elf.defined_symbols.append({"name": name, "value": 0x08000301,
                    "size": 16, "section": 0, "type": 2, "binding": 1})
                self.inspect(elf)

    def test_four_byte_pool_alignment_is_valid_except_for_tls_arena(self):
        elf = image()
        for symbol in elf.defined_symbols:
            if (symbol["type"] == 1 and symbol["section"] == 1 and
                    symbol["name"] not in {"tls_arena", "s_nx_port_random"}):
                symbol["value"] += 4
        self.assertEqual(self.inspect(elf)["status"], "passed")

    def test_output_cannot_delete_any_canonical_or_copied_secure_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            canonical = root / "cmake/package/verification/secure_services"
            shutil.copytree(ROOT / "cmake/package/verification/secure_services", canonical)
            copied = root / "consumer"
            shutil.copytree(canonical / "f407", copied)
            shutil.copyfile(canonical / "public_ca.h", copied / "public_ca.h")
            (root / "resolved.json").write_text("{}\n")
            for output in (canonical / "public_ca.h", canonical / "f407/application.c",
                           canonical / "f407/assembly.toml", copied / "public_ca.h"):
                with self.subTest(output=output):
                    before = output.read_bytes()
                    with patch.object(retention, "ROOT", root):
                        result = retention.main([
                            "--elf", str(root / "not-read.elf"), "--bindings",
                            str(root / "not-read-bindings.c"), "--resolved",
                            str(root / "resolved.json"), "--component-owner",
                            "f407-secure-services", "--consumer",
                            str(copied / "application.c"), "--output", str(output)])
                    self.assertEqual(result, 1)
                    self.assertTrue(output.exists(), "retention deleted a protected input")
                    self.assertEqual(output.read_bytes(), before)

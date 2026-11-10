"""Official ST USB sources remain pinned, organization-owned and optional."""
import json
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from tools.maintenance import source_catalogue
from tools.maintenance import st_usb_lock

ROOT = Path(source_catalogue.__file__).resolve().parents[2]


class StUsbInventoryTests(unittest.TestCase):
    RELEASES = (
        ("stm32_usb_device", "usb_device", "stm32-mw-usb-device", "2.11.4",
         "2a0a3521ac4d84e6e494d37bed615e2d36c373f5"),
        ("stm32_usb_host", "usb_host", "stm32-mw-usb-host", "3.5.3",
         "9d46e44c858a77bf126920b7bcc258d59afa89b1"),
    )

    def test_official_usb_sources_have_organization_transport_and_inventory_scope(self):
        for identifier, folder, repository, _, _ in self.RELEASES:
            with self.subTest(identifier=identifier):
                item = source_catalogue.dependency(identifier)
                self.assertEqual(item.path, "vendors/st/" + folder)
                self.assertEqual(item.scope, "runtime")
                self.assertEqual(item.notice, "LICENSE.md")
                self.assertEqual(item.upstream_url,
                                 "https://github.com/STMicroelectronics/" +
                                 repository + ".git")
                self.assertEqual(item.fetch_url,
                                 "https://github.com/X-Gen-Lab/" +
                                 repository + ".git")

    def test_no_existing_f4_runtime_tuple_prepares_the_unselected_middleware(self):
        inventory = {source_catalogue.dependency(row[0]).path
                     for row in self.RELEASES}
        for backend in ("baremetal", "freertos", "threadx"):
            for components in ((), ("network", "tls", "mqtt", "mcuboot")):
                with self.subTest(backend=backend, components=components):
                    selected = source_catalogue.runtime_selection(
                        "stm32f4", backend=backend, components=components)
                    self.assertFalse(inventory & {item.path for item in selected})

    def test_explicit_host_component_selects_only_the_host_middleware(self):
        selected = source_catalogue.runtime_selection(
            "stm32f4", backend="baremetal", components=("st-usb-host",))
        paths = {item.path for item in selected}
        self.assertIn("vendors/st/usb_host", paths)
        self.assertNotIn("vendors/st/usb_device", paths)

    def test_explicit_device_component_selects_only_device_middleware(self):
        selected = source_catalogue.runtime_selection(
            "stm32f4", backend="baremetal", components=("st-usb-device",))
        paths = {item.path for item in selected}
        self.assertIn("vendors/st/usb_device", paths)
        self.assertNotIn("vendors/st/usb_host", paths)

    def test_gitlinks_and_locks_bind_the_reviewed_official_cube_tuple(self):
        hal = json.loads((ROOT / "dependencies/stm32f4-hal.lock.json").read_text())
        for identifier, _, _, version, commit in self.RELEASES:
            with self.subTest(identifier=identifier):
                item = source_catalogue.dependency(identifier)
                locked = json.loads((ROOT / item.source_lock).read_text())
                self.assertEqual(locked["version"], version)
                self.assertEqual(locked["commit"], commit)
                self.assertEqual(locked["license"], "LicenseRef-ST-SLA0044")
                self.assertEqual(locked["pairing"], hal["pairing"])
                output = subprocess.run(
                    ["git", "ls-files", "--stage", "--", item.path],
                    cwd=ROOT, capture_output=True, text=True, check=True).stdout
                self.assertEqual(output.split()[:2], ["160000", commit])
                self.assertIn("LICENSE.md", locked["files"])

    def test_explicit_middleware_admission_checks_actual_locked_bytes(self):
        for role, row in zip(("device", "host"), self.RELEASES):
            with self.subTest(role=role):
                identity = st_usb_lock.verify(ROOT, role)
                self.assertEqual(identity["commit"], row[4])
                self.assertEqual(identity["version"], row[3])
                filename = "usbh_core.c" if role == "host" else "usbd_core.c"
                self.assertIn(str(ROOT / "vendors/st" / ("usb_" + role) /
                                  "Core/Src" / filename),
                              identity["configure_inputs"])

    def test_middleware_admission_refuses_wrong_role_and_tampered_file_map(self):
        with self.assertRaises(ValueError):
            st_usb_lock.verify(ROOT, "otg")
        original = (ROOT / "dependencies/stm32-usb-device.lock.json").read_bytes()
        lock = json.loads(original)
        lock["files"].pop("Core/Src/usbd_core.c")
        with patch("tools.maintenance.st_usb_lock.read_lock", return_value=lock):
            with self.assertRaisesRegex(ValueError, "file map"):
                st_usb_lock.verify(ROOT, "device")


if __name__ == "__main__":
    unittest.main()

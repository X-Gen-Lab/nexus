"""Execute mutually exclusive lwIP profile admission through real CMake."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
NETWORK_COMPONENT = ROOT / "components/network"
LWIP_SOURCE = ROOT / "ext/lwip"
CMAKE_ARGUMENTS = ()


class LwIPProfilesTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="lwIP profiles ")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def configure(self, libraries):
        source = self.directory / "consumer source"
        source.mkdir()
        (source / "main.c").write_text("int main(void) { return 0; }\n")
        # Prerequisite interfaces provide graph identities only. The component,
        # target policy and pinned-source verifier are the maintained files.
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.31)\n'
            'project(lwip_profile_admission C)\n'
            'set(CMAKE_C_STANDARD 11)\n'
            'set(CMAKE_C_EXTENSIONS OFF)\n'
            f'set(NEXUS_SOURCE_DIR "{ROOT.as_posix()}")\n'
            f'set(Python3_EXECUTABLE "{Path(sys.executable).as_posix()}")\n'
            'add_library(fixture_config INTERFACE)\n'
            'add_library(Nexus::Config ALIAS fixture_config)\n'
            'add_library(fixture_io INTERFACE)\n'
            'add_library(Nexus::IO ALIAS fixture_io)\n'
            'add_library(fixture_io_ethernet_frames INTERFACE)\n'
            'add_library(Nexus::IOEthernetFrames ALIAS fixture_io_ethernet_frames)\n'
            'add_library(nexus_abi_options INTERFACE)\n'
            'add_library(nexus_build_options INTERFACE)\n'
            'add_library(nexus_owned_warnings INTERFACE)\n'
            f'include("{ROOT.as_posix()}/cmake/platform/TargetPolicy.cmake")\n'
            f'add_subdirectory("{NETWORK_COMPONENT.as_posix()}" network)\n'
            'add_executable(consumer main.c)\n'
            'target_link_libraries(consumer PRIVATE ' + libraries + ')\n')
        return subprocess.run([
            shutil.which("cmake") or "cmake", "-S", str(source),
            "-B", str(self.directory / "build output"), "-G", "Ninja",
            *CMAKE_ARGUMENTS],
            capture_output=True, text=True, check=False, timeout=60)

    def test_existing_copy_profile_is_a_valid_control(self):
        result = self.configure("Nexus::LwIPTCPStorage")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_selected_loan_profile_configures(self):
        result = self.configure("Nexus::LwIPTCPLoanedRXStorage")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_copy_and_loan_profiles_cannot_enter_one_image(self):
        result = self.configure(
            "Nexus::LwIPStorage Nexus::LwIPLoanedRXStorage")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("NEXUS_LWIP_RX_PROFILE", result.stdout + result.stderr)

    def test_copy_tcp_cannot_silently_choose_loaned_adapter_layout(self):
        result = self.configure("Nexus::LwIPTCP Nexus::LwIPLoanedRXStorage")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("NEXUS_LWIP_RX_PROFILE", result.stdout + result.stderr)


class LwIPSanityCompileTests(unittest.TestCase):
    """Compile the real supplier condition with deliberately invalid ports."""

    def compile_case(self, *, pool, custom=False, exemption=None,
                     invalid_segments=False):
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "Prepared Native C compiler required")
        with tempfile.TemporaryDirectory(prefix="lwIP sanity ") as temporary:
            directory = Path(temporary)
            options = (
                '#include "' + (NETWORK_COMPONENT / "port/lwipopts.h").as_posix()
                + '"\n#undef PBUF_POOL_SIZE\n#define PBUF_POOL_SIZE '
                + str(pool) + '\n#undef NEXUS_LWIP_RX_CUSTOM_PBUFS\n')
            if exemption is not None:
                options += ('#define NEXUS_LWIP_RX_CUSTOM_PBUFS '
                            + str(exemption) + '\n')
            if invalid_segments:
                options += ('#undef MEMP_NUM_TCP_SEG\n'
                            '#define MEMP_NUM_TCP_SEG 1\n')
            (directory / "lwipopts.h").write_text(options)
            arguments = [compiler, "-std=c11", "-Os", "-I" + str(directory),
                         "-I" + str(NETWORK_COMPONENT / "port"), "-isystem",
                         str(LWIP_SOURCE / "src/include")]
            if custom:
                arguments += ["-DLWIP_SUPPORT_CUSTOM_PBUF=1"]
            return subprocess.run(
                arguments + ["-c", str(LWIP_SOURCE / "src/core/init.c"),
                             "-o", str(directory / "init.o")],
                capture_output=True, text=True, check=False, timeout=60)

    def rejected(self, result, diagnostic):
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(diagnostic, result.stdout + result.stderr)

    def test_default_pool_still_compiles(self):
        result = self.compile_case(pool=24)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_copy_pool_cannot_undersize_its_receive_window(self):
        self.rejected(self.compile_case(pool=2), "TCP_WND is larger")

    def test_custom_pbuf_alone_does_not_disable_the_pool_check(self):
        self.rejected(self.compile_case(pool=2, custom=True),
                      "TCP_WND is larger")

    def test_only_explicit_custom_profile_exempts_the_pool_heuristic(self):
        result = self.compile_case(pool=2, custom=True, exemption=1)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_zero_exemption_keeps_the_pool_check(self):
        self.rejected(self.compile_case(pool=2, custom=True, exemption=0),
                      "TCP_WND is larger")

    def test_exemption_without_custom_pbuf_keeps_the_pool_check(self):
        self.rejected(self.compile_case(pool=2, exemption=1),
                      "TCP_WND is larger")

    def test_explicit_custom_profile_keeps_other_tcp_sanity_checks(self):
        self.rejected(self.compile_case(pool=2, custom=True, exemption=1,
                                        invalid_segments=True),
                      "MEMP_NUM_TCP_SEG should be at least")


if __name__ == "__main__":
    unittest.main()

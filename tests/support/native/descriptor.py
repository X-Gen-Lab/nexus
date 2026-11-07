"""Exact native integration metadata; CMake owns sources."""
from tools.configure.providers.descriptor import FamilyDescriptor

DESCRIPTOR = FamilyDescriptor(
    family="native",
    vendor="native",
    descriptor_path="tests/support/native/descriptor.py",
    source_directory="tests/support/native",
    configuration_directory="tests/support/native/configure",
    owned_payload=(
        "tests/support/native/CMakeLists.txt",
        "io/include/nexus/io/rtc.h",
        "io/include/nexus/io/lptim.h",
        "io/include/nexus/io/native/rtc_lptim_model.h",
        "tests/support/native/drivers/rtc_lptim_provider.h",
        "tests/support/native/drivers/rtc_lptim.c",
        "io/private/rtc_lptim_internal.h",
        "io/src/api/rtc.c",
        "io/src/api/lptim.c",
        "tests/support/native/board/board.json",
        "cmake/package/verification/io_evolution/native-time-pair.toml",
        "cmake/package/verification/io_evolution/native-time-empty.toml",
        "cmake/package/verification/io_evolution/consumers/native-time-pair.c",
        "cmake/package/verification/io_evolution/consumers/native-time-empty.c",
    ),
)

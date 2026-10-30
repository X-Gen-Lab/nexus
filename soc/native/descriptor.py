"""Exact native integration metadata; CMake owns sources."""
from tools.configure.providers.descriptor import FamilyDescriptor

DESCRIPTOR = FamilyDescriptor(
    family="native",
    vendor="native",
    source_directory="soc/native",
    configuration_directory="soc/native/configure",
    owned_payload=(
        "soc/native/CMakeLists.txt",
        "io/include/nexus/io/rtc.h",
        "io/include/nexus/io/lptim.h",
        "io/include/nexus/io/native/rtc_lptim_model.h",
        "soc/native/drivers/rtc_lptim_provider.h",
        "soc/native/drivers/rtc_lptim.c",
        "io/private/rtc_lptim_internal.h",
        "io/src/api/rtc.c",
        "io/src/api/lptim.c",
        "boards/native_reference/board.json",
        "cmake/package/verification/io_evolution/native-time-pair.toml",
        "cmake/package/verification/io_evolution/native-time-empty.toml",
        "cmake/package/verification/io_evolution/consumers/native-time-pair.c",
        "cmake/package/verification/io_evolution/consumers/native-time-empty.c",
    ),
)

cmake_minimum_required(VERSION 3.31)
project(f407_i2c_target_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(f407_i2c_target SOURCES main.c)
nexus_target_warnings(f407_i2c_target STRICT)

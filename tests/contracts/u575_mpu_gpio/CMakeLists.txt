cmake_minimum_required(VERSION 3.24)
project(nexus_u575_mpu_gpio_public LANGUAGES C ASM)
if(NOT NEXUS_SOURCE_ROOT)
    message(FATAL_ERROR "An explicit Nexus source root is required")
endif()
set(NEXUS_ASSEMBLY_FILE
    "${NEXUS_SOURCE_ROOT}/tools/configure/assemblies/u575-mpu-gpio-write.toml")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus)
nexus_add_firmware(u575_mpu_gpio_public
    SOURCES "${NEXUS_SOURCE_ROOT}/cmake/package/verification/io_evolution/consumers/u575-mpu-gpio.c"
    LIBRARIES Nexus::U575MpuGpio)
set_target_properties(u575_mpu_gpio_public PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
nexus_target_warnings(u575_mpu_gpio_public STRICT)

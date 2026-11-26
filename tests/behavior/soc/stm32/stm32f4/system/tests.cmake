# Explicit system behavior and private model dependencies.
# Author: Nexus Team
include(
    "${nexus_google_root}/soc/stm32/stm32f4/system/stm32_platform_cold.cmake"
)

include(
    "${nexus_google_root}/soc/stm32/stm32f4/system/stm32_static_lifecycle.cmake"
)

include(
    "${nexus_google_root}/soc/stm32/stm32f4/system/stm32_control_factory.cmake"
)

include("${nexus_google_root}/soc/stm32/stm32f4/system/stm32_basic_cold.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/system/qiming_touch.cmake")

include(
    "${nexus_google_root}/soc/stm32/stm32f4/system/qiming_boot_platform.cmake"
)

nexus_google_test(nexus_silicon_admission_test
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/system/silicon_admission_test.cpp"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

nexus_google_test(nexus_stm32_platform_coordinator_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/system/stm32_platform_coordinator_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/platform_coordinator.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/tests/support/arm/stm32f4"
        "${NEXUS_SOURCE_DIR}/arch/include"
)

target_include_directories(
    nexus_stm32_platform_coordinator_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)

target_compile_definitions(
    nexus_stm32_platform_coordinator_test
    PRIVATE STM32F407xx
)

include(
    "${nexus_google_root}/soc/stm32/stm32f4/system/multi_controller_factory.cmake"
)

include(
    "${nexus_google_root}/soc/stm32/stm32f4/system/physical_platform_factory.cmake"
)

include("${nexus_google_root}/soc/stm32/stm32f4/system/soc_system.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/system/stm32_official.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/system/stm32_pvd.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/system/stm32f407_handoff.cmake")

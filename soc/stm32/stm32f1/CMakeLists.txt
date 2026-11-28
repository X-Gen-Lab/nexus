# One package-local build graph for the selected F1 assembly.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/cmake/platform/CMSISCore.cmake")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")

if(
    NOT NEXUS_CPU_ARCH STREQUAL "cortex-m3"
    OR NOT NEXUS_VENDOR_DEVICE_DEFINE STREQUAL "STM32F103xB"
    OR NOT NEXUS_VENDOR_DEVICE_HEADER
    OR NOT NEXUS_VENDOR_DEVICE_STARTUP
)
    message(FATAL_ERROR "STM32F1 requires the reviewed Cortex-M3 xB profile")
endif()
set(f1_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1")
set(f1_hal "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal")
include("${CMAKE_CURRENT_LIST_DIR}/sdk/Official.cmake")
nexus_stm32f1_official()

add_library(nexus_soc_stm32f1 STATIC system/platform.c)
target_include_directories(
    nexus_soc_stm32f1
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${NEXUS_SOURCE_DIR}/io/private"
)
target_include_directories(
    nexus_soc_stm32f1
    SYSTEM
    PRIVATE "${f1_device}/Include" "${f1_hal}/Inc"
)
target_compile_definitions(
    nexus_soc_stm32f1
    PRIVATE ${NEXUS_VENDOR_DEVICE_DEFINE}
)
target_link_libraries(
    nexus_soc_stm32f1
    PUBLIC nexus_io
    PRIVATE Nexus::Arch Nexus::Core nexus_build_options nexus_owned_warnings
)
nexus_use_cmsis_core(nexus_soc_stm32f1)

nexus_controller_modes(f1_gpio gpio)
nexus_controller_modes(f1_uart uart)
if(f1_gpio OR f1_uart)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/gpio/gpio.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOGPIO)
endif()
if(f1_uart)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/uart/uart.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOUART)
endif()
add_subdirectory(drivers/flash)

# Only the image's startup and generated bindings consume these requirements.
add_library(nexus_stm32f1_startup_usage INTERFACE)
target_include_directories(
    nexus_stm32f1_startup_usage
    INTERFACE "${CMAKE_CURRENT_LIST_DIR}"
)
target_include_directories(
    nexus_stm32f1_startup_usage
    SYSTEM
    INTERFACE "${f1_device}/Include" "${f1_hal}/Inc"
)
target_compile_definitions(
    nexus_stm32f1_startup_usage
    INTERFACE ${NEXUS_VENDOR_DEVICE_DEFINE}
)
nexus_use_cmsis_core(nexus_stm32f1_startup_usage)
set_property(
    TARGET nexus_soc_stm32f1
    PROPERTY NEXUS_STARTUP_USAGE_TARGET nexus_stm32f1_startup_usage
)
set_property(
    TARGET nexus_soc_stm32f1
    PROPERTY
        NEXUS_STARTUP_SOURCES
            "${f1_device}/${NEXUS_VENDOR_DEVICE_STARTUP}"
            "${CMAKE_CURRENT_LIST_DIR}/system/startup.c"
            "${CMAKE_CURRENT_LIST_DIR}/system/runtime.c"
)

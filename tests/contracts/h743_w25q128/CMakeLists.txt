cmake_minimum_required(VERSION 3.31)
project(h743_w25q128_link_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "The reference needs an explicit source and assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "ARM link contract has no host tests")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit public product source")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
if(NOT TARGET Nexus::H743W25Q128MappedStorage)
    message(FATAL_ERROR "The reference requires the exact H743 component")
endif()

# Only the composition target sees vendor types and private construction APIs.
add_library(h743_w25q128_composition STATIC composition.c)
nexus_apply_target_policy(h743_w25q128_composition)
target_include_directories(h743_w25q128_composition PRIVATE
    "${NEXUS_SOURCE_ROOT}/soc/stm32/stm32h743/private"
    "${NEXUS_SOURCE_ROOT}/components/soc/stm32/stm32h743_w25q128/private")
target_include_directories(h743_w25q128_composition SYSTEM PRIVATE
    "${NEXUS_SOURCE_ROOT}/vendors/st/cmsis_device_h7/Include")
nexus_use_cmsis_core(h743_w25q128_composition)
nexus_stm32_official(h743_w25q128_composition h7)
target_compile_definitions(h743_w25q128_composition PRIVATE STM32H743xx)
target_link_libraries(h743_w25q128_composition PUBLIC Nexus::MappedStorage
    PRIVATE Nexus::H743W25Q128MappedStorage nexus_owned_warnings)

nexus_add_firmware(h743_w25q128_consumer
    SOURCES main.c private_headers.c LIBRARIES h743_w25q128_composition)
# Library-position option follows inherited link options, proving the complete
# retained supplier object closes without relying on discarded sections.
target_link_libraries(h743_w25q128_consumer PRIVATE "-Wl,--no-gc-sections")

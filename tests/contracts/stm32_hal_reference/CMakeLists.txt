# Explicit Linux-only behavior comparison; never part of firmware or the SDK.
cmake_minimum_required(VERSION 3.31.6)
project(nexus_stm32_hal_reference LANGUAGES C CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "The HAL register reference requires a Linux host")
endif()
get_filename_component(NEXUS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
    ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
set(official "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal")
set(cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
set(device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
foreach(name uart dma)
    if(NOT EXISTS "${official}/Src/stm32f4xx_hal_${name}.c")
        message(FATAL_ERROR "HAL comparison needs the full locked Git checkout")
    endif()
endforeach()

include("${NEXUS_SOURCE_DIR}/cmake/testing/GoogleTest.cmake")
nexus_host_google_test()

include("${CMAKE_CURRENT_SOURCE_DIR}/reference.cmake")
add_executable(nexus_stm32_hal_reference reference_test.cpp)
nexus_stm32_hal_reference(nexus_stm32_hal_reference)
target_link_libraries(nexus_stm32_hal_reference PRIVATE GTest::gmock_main)
set(NEXUS_REFERENCE_SANITIZER "none" CACHE STRING "none, address or thread")
if(NOT NEXUS_REFERENCE_SANITIZER MATCHES "^(none|address|thread)$")
    message(FATAL_ERROR "Unknown reference sanitizer")
endif()
if(NOT NEXUS_REFERENCE_SANITIZER STREQUAL "none")
    foreach(target nexus_stm32_hal_reference_vendor nexus_stm32_hal_reference
                   gtest gmock gmock_main)
        target_compile_options(${target} PRIVATE
            "-fsanitize=${NEXUS_REFERENCE_SANITIZER}" -fno-omit-frame-pointer)
    endforeach()
    target_link_options(nexus_stm32_hal_reference PRIVATE
        "-fsanitize=${NEXUS_REFERENCE_SANITIZER}")
endif()
enable_testing()
add_test(NAME nexus_stm32_hal_reference COMMAND nexus_stm32_hal_reference
    "--gtest_output=xml:${CMAKE_BINARY_DIR}/reference.xml")
set_tests_properties(nexus_stm32_hal_reference PROPERTIES TIMEOUT 30)

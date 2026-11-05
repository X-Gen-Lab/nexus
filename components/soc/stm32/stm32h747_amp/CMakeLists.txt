# Explicit external paired-boot attachment; no global clock/startup owner.
if(NOT NEXUS_SOC_FAMILY STREQUAL "stm32h747" OR
        NOT NEXUS_BACKEND STREQUAL "baremetal")
    return()
endif()
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
add_library(nexus_stm32h747_amp STATIC EXCLUDE_FROM_ALL
    src/stm32h747_amp.c src/hardware.c)
nexus_apply_target_policy(nexus_stm32h747_amp)
target_include_directories(nexus_stm32h747_amp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_stm32h747_amp SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc")
nexus_use_cmsis_core(nexus_stm32h747_amp)
nexus_stm32_official(nexus_stm32h747_amp h7)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(h747_device_lock "${NEXUS_SOURCE_DIR}/dependencies/stm32h747.lock.json")
set(h747_device_verifier
    "${NEXUS_SOURCE_DIR}/tools/maintenance/cmsis_device_lock.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${h747_device_verifier}"
    --source "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7"
    --lock "${h747_device_lock}"
    RESULT_VARIABLE h747_verified ERROR_VARIABLE h747_error OUTPUT_QUIET)
if(NOT h747_verified EQUAL 0)
    message(FATAL_ERROR "H747 device source identity failed: ${h747_error}")
endif()
file(READ "${h747_device_lock}" h747_lock)
string(JSON h747_file_count LENGTH "${h747_lock}" files)
math(EXPR h747_last "${h747_file_count} - 1")
set(h747_device_inputs "${h747_device_lock}" "${h747_device_verifier}")
foreach(h747_index RANGE 0 ${h747_last})
    string(JSON h747_name MEMBER "${h747_lock}" files ${h747_index})
    list(APPEND h747_device_inputs
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/${h747_name}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${h747_device_inputs})
target_link_libraries(nexus_stm32h747_amp PUBLIC Nexus::AMPIPC
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::H747AMP ALIAS nexus_stm32h747_amp)

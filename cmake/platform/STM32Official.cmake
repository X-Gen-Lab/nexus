# LL primitives and the explicit F407 HAL transport have private build scopes.
# Every consuming entry verifies current bytes; a previous success is no cache.
include_guard(GLOBAL)

function(nexus_stm32f4_official target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "STM32F4 official LL requires one existing target")
    endif()
    get_target_property(kind "${target}" TYPE)
    get_target_property(imported "${target}" IMPORTED)
    get_target_property(alias "${target}" ALIASED_TARGET)
    if(kind STREQUAL "INTERFACE_LIBRARY" OR imported OR alias)
        message(FATAL_ERROR "STM32F4 official LL requires an owned compile target")
    endif()
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${source}/tools/maintenance/stm32f4_hal_lock.py"
        --source "${source}/vendors/st/stm32f4_hal"
        --lock "${source}/dependencies/stm32f4-hal.lock.json"
        --cmsis-source "${source}/vendors/st/cmsis_device_f4"
        RESULT_VARIABLE result ERROR_VARIABLE error OUTPUT_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "STM32F4 official LL identity failed: ${error}")
    endif()
    target_include_directories("${target}" SYSTEM PRIVATE
        "${source}/vendors/st/stm32f4_hal/Inc")
    set_target_properties("${target}" PROPERTIES
        NEXUS_STM32F4_OFFICIAL_REVISION
        "b6f0ed3829f3829eb358a2e7417d80bba1a42db7")
    # The reviewed lock owns the selected file map; this does not select any
    # source or become a second build graph. Changes retrigger verification.
    file(READ "${source}/dependencies/stm32f4-hal.lock.json" lock)
    string(JSON count LENGTH "${lock}" files)
    math(EXPR last "${count} - 1")
    set(inputs "${source}/dependencies/stm32f4-hal.lock.json"
        "${source}/tools/maintenance/stm32f4_hal_lock.py"
        "${source}/tools/maintenance/cmsis_device_lock.py"
        "${source}/tools/configure/metadata.py")
    foreach(index RANGE 0 ${last})
        string(JSON name MEMBER "${lock}" files ${index})
        list(APPEND inputs "${source}/vendors/st/stm32f4_hal/${name}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${inputs})
endfunction()

# Only the selected F407 finite TX mode owns this UART/DMA runtime. Insert the
# objects in the consumer archive, avoiding a second transport or link cycle.
function(nexus_stm32f407_uart_hal target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "STM32F407 UART HAL requires one owned target")
    endif()
    nexus_stm32f4_official("${target}")
    set(vendor "${target}_stm32f407_hal")
    if(TARGET "${vendor}")
        message(FATAL_ERROR "STM32F407 UART HAL transport already selected")
    endif()
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(private "${source}/soc/stm32/stm32f407/private")
    add_library("${vendor}" OBJECT
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_uart.c"
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_dma.c"
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_rcc.c")
    nexus_stm32f4_official("${vendor}")
    target_compile_features("${vendor}" PRIVATE c_std_11)
    set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions("${vendor}" PRIVATE STM32F407xx)
    target_include_directories("${vendor}" PRIVATE "${private}"
        "${private}/hal" "${source}/arch/include")
    target_include_directories("${vendor}" SYSTEM PRIVATE
        "${source}/vendors/st/cmsis_device_f4/Include")
    include("${source}/cmake/platform/CMSISCore.cmake")
    nexus_use_cmsis_core("${vendor}")
    foreach(policy nexus_build_options Nexus::Arch)
        if(TARGET "${policy}")
            target_link_libraries("${vendor}" PRIVATE "${policy}")
        endif()
    endforeach()
    target_compile_options("${vendor}" PRIVATE
        "-include${private}/stm32f407_hal_profile.h"
        -ffunction-sections -fdata-sections)
    target_include_directories("${target}" PRIVATE "${private}/hal")
    target_sources("${target}" PRIVATE "$<TARGET_OBJECTS:${vendor}>")
endfunction()

# Family-specific profiles share immutable-source verification, never register
# layout, transfer state or hardware ownership. No vendor C runtime is selected.
function(nexus_stm32_official target family)
    if(NOT ARGC EQUAL 2 OR NOT TARGET "${target}" OR
            NOT family MATCHES "^(g4|h7|u5)$")
        message(FATAL_ERROR "Official LL requires one owned target and reviewed family")
    endif()
    get_target_property(kind "${target}" TYPE)
    get_target_property(imported "${target}" IMPORTED)
    get_target_property(alias "${target}" ALIASED_TARGET)
    if(kind STREQUAL "INTERFACE_LIBRARY" OR imported OR alias)
        message(FATAL_ERROR "Official LL requires an owned compile target")
    endif()
    get_filename_component(source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(lock_path "${source}/dependencies/stm32${family}-hal.lock.json")
    set(verifier "${source}/tools/maintenance/stm32${family}_hal_lock.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${verifier}"
        --source "${source}/vendors/st/stm32${family}_hal" --lock "${lock_path}"
        --cmsis-source "${source}/vendors/st/cmsis_device_${family}"
        RESULT_VARIABLE result ERROR_VARIABLE error OUTPUT_QUIET)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Official ${family} LL identity failed: ${error}")
    endif()
    file(READ "${lock_path}" lock)
    string(JSON revision GET "${lock}" commit)
    target_include_directories("${target}" SYSTEM PRIVATE
        "${source}/vendors/st/stm32${family}_hal/Inc")
    set_target_properties("${target}" PROPERTIES
        NEXUS_STM32_OFFICIAL_REVISION "${revision}")
    string(JSON count LENGTH "${lock}" files)
    math(EXPR last "${count} - 1")
    set(inputs "${lock_path}" "${verifier}"
        "${source}/tools/maintenance/stm32_ll_lock.py"
        "${source}/tools/maintenance/cmsis_device_lock.py"
        "${source}/tools/configure/metadata.py")
    foreach(index RANGE 0 ${last})
        string(JSON name MEMBER "${lock}" files ${index})
        list(APPEND inputs "${source}/vendors/st/stm32${family}_hal/${name}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${inputs})
endfunction()

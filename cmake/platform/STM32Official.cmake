# Reviewed LL primitives and explicit HAL selections have private build scopes.
# One configure verifies exact inputs at first use and again after all deferred
# work. Targets share only that pending input snapshot, never a past success.
include_guard(GLOBAL)

function(_nexus_stm32f4_finish_inputs)
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" GET_CALL_IDS remaining)
    list(LENGTH remaining remaining_count)
    if(remaining_count GREATER 0)
        # A later callback can itself enqueue work. Verify only at the actual
        # tail of root configure, including first use from a child directory.
        cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
            CALL _nexus_stm32f4_finish_inputs)
        return()
    endif()
    get_property(source GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SOURCE)
    get_property(python GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_PYTHON)
    get_property(pending GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SNAPSHOT)
    get_property(digest GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_DIGEST)
    execute_process(COMMAND "${python}" -B
        "${source}/tools/maintenance/stm32f4_hal_lock.py"
        --source "${source}/vendors/st/stm32f4_hal"
        --lock "${source}/dependencies/stm32f4-hal.lock.json"
        --cmsis-source "${source}/vendors/st/cmsis_device_f4"
        --end-configure "${pending}" --snapshot-sha256 "${digest}"
        RESULT_VARIABLE result ERROR_VARIABLE error OUTPUT_QUIET)
    file(REMOVE "${pending}")
    set_property(GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SNAPSHOT "")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "STM32F4 final official identity failed: ${error}")
    endif()
endfunction()

function(_nexus_stm32f4_admit_inputs source)
    get_property(pending GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SNAPSHOT)
    if(pending)
        get_property(admitted GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SOURCE)
        if(NOT admitted STREQUAL source)
            message(FATAL_ERROR "STM32F4 configure input source changed")
        endif()
        return()
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(directory "${CMAKE_BINARY_DIR}/CMakeFiles/nexus-official")
    file(MAKE_DIRECTORY "${directory}")
    string(RANDOM LENGTH 32 ALPHABET 0123456789abcdef nonce)
    set(pending "${directory}/${nonce}.pending.json")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${source}/tools/maintenance/stm32f4_hal_lock.py"
        --source "${source}/vendors/st/stm32f4_hal"
        --lock "${source}/dependencies/stm32f4-hal.lock.json"
        --cmsis-source "${source}/vendors/st/cmsis_device_f4"
        --begin-configure "${pending}"
        RESULT_VARIABLE result ERROR_VARIABLE error OUTPUT_VARIABLE identity)
    if(NOT result EQUAL 0)
        file(REMOVE "${pending}")
        message(FATAL_ERROR "STM32F4 official LL identity failed: ${error}")
    endif()
    string(JSON digest GET "${identity}" configure_snapshot_sha256)
    string(LENGTH "${digest}" length)
    if(NOT length EQUAL 64 OR NOT digest MATCHES "^[0-9a-f]+$")
        file(REMOVE "${pending}")
        message(FATAL_ERROR "STM32F4 configure input digest is invalid")
    endif()
    # One projection of the admitted inputs owns configure dependencies too.
    # Exact absent candidates observe packed/loose refs and optional Git files
    # without depending on directories touched by ordinary Git lock traffic.
    string(JSON count LENGTH "${identity}" configure_inputs)
    if(count LESS 1)
        file(REMOVE "${pending}")
        message(FATAL_ERROR "STM32F4 configure inputs are empty")
    endif()
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON path GET "${identity}" configure_inputs ${index})
        if(NOT IS_ABSOLUTE "${path}" OR NOT EXISTS "${path}" OR
                IS_DIRECTORY "${path}" OR IS_SYMLINK "${path}")
            file(REMOVE "${pending}")
            message(FATAL_ERROR "STM32F4 configure input is not a regular file")
        endif()
        set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS "${path}")
    endforeach()
    string(JSON count LENGTH "${identity}" configure_file_probes)
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON pattern GET "${identity}" configure_file_probes ${index})
            if(NOT IS_ABSOLUTE "${pattern}")
                file(REMOVE "${pending}")
                message(FATAL_ERROR "STM32F4 configure probe must be absolute")
            endif()
            file(GLOB candidate LIST_DIRECTORIES false CONFIGURE_DEPENDS
                "${pattern}")
        endforeach()
    endif()
    set_property(GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SOURCE "${source}")
    set_property(GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_PYTHON "${Python3_EXECUTABLE}")
    set_property(GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_SNAPSHOT "${pending}")
    set_property(GLOBAL PROPERTY _NEXUS_STM32F4_INPUT_DIGEST "${digest}")
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
        CALL _nexus_stm32f4_finish_inputs)
endfunction()

function(nexus_stm32f4_device_definition target output)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "STM32F4 device requires one owned compile target")
    endif()
    get_target_property(definitions "${target}" COMPILE_DEFINITIONS)
    if(NOT definitions)
        set(definitions)
    endif()
    list(FILTER definitions INCLUDE REGEX "^STM32F4[A-Za-z0-9]+$")
    if(NEXUS_STM32_DEVICE)
        list(APPEND definitions "${NEXUS_STM32_DEVICE}")
    endif()
    list(REMOVE_DUPLICATES definitions)
    list(LENGTH definitions count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "STM32F4 target requires one exact device definition")
    endif()
    list(GET definitions 0 device)
    set("${output}" "${device}" PARENT_SCOPE)
endfunction()

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
    _nexus_stm32f4_admit_inputs("${source}")
    target_include_directories("${target}" SYSTEM PRIVATE
        "${source}/vendors/st/stm32f4_hal/Inc")
    set_target_properties("${target}" PROPERTIES
        NEXUS_STM32F4_OFFICIAL_REVISION
        "b6f0ed3829f3829eb358a2e7417d80bba1a42db7")
endfunction()

# One official DMA object per owning SoC target, shared by selected transports.
function(nexus_stm32f4_dma_hal target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "STM32F4 DMA HAL requires one owned target")
    endif()
    nexus_stm32f4_device_definition("${target}" device)
    nexus_stm32f4_official("${target}")
    set(vendor "${target}_stm32_dma_hal")
    if(TARGET "${vendor}")
        nexus_stm32f4_device_definition("${vendor}" selected)
        if(NOT selected STREQUAL device)
            message(FATAL_ERROR "STM32F4 DMA HAL device changed")
        endif()
        return()
    endif()
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    add_library("${vendor}" OBJECT
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_dma.c")
    nexus_stm32f4_official("${vendor}")
    target_compile_features("${vendor}" PRIVATE c_std_11)
    set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions("${vendor}" PRIVATE "${device}")
    target_include_directories("${vendor}" PRIVATE
        "${source}/soc/stm32/stm32f4/private/dma_hal")
    target_include_directories("${vendor}" SYSTEM PRIVATE
        "${source}/vendors/st/cmsis_device_f4/Include")
    include("${source}/cmake/platform/CMSISCore.cmake")
    nexus_use_cmsis_core("${vendor}")
    if(COMMAND nexus_use_cmsis_host_model AND NOT CMAKE_CROSSCOMPILING)
        nexus_use_cmsis_host_model("${vendor}")
    endif()
    foreach(policy nexus_build_options Nexus::Arch)
        if(TARGET "${policy}")
            target_link_libraries("${vendor}" PRIVATE "${policy}")
        endif()
    endforeach()
    target_compile_options("${vendor}" PRIVATE
        -ffunction-sections -fdata-sections)
    target_sources("${target}" PRIVATE "$<TARGET_OBJECTS:${vendor}>")
endfunction()

# Share build objects only; System remains the sole runtime RCC owner.
function(nexus_stm32f4_rcc_hal target)
    if(NOT TARGET "${target}" OR ARGC GREATER 2
        OR (ARGC EQUAL 2 AND NOT "${ARGV1}" STREQUAL "EXTENDED"))
        message(FATAL_ERROR "STM32F4 RCC HAL requires one target and optional EXTENDED")
    endif()
    nexus_stm32f4_device_definition("${target}" device)
    nexus_stm32f4_official("${target}")
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(vendor "${target}_stm32_rcc_hal")
    if(TARGET "${vendor}")
        nexus_stm32f4_device_definition("${vendor}" selected)
        if(NOT selected STREQUAL device)
            message(FATAL_ERROR "STM32F4 RCC HAL device changed")
        endif()
    else()
        add_library("${vendor}" OBJECT
            "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_rcc.c")
        nexus_stm32f4_official("${vendor}")
        target_compile_features("${vendor}" PRIVATE c_std_11)
        set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
        target_compile_definitions("${vendor}" PRIVATE "${device}")
        target_include_directories("${vendor}" PRIVATE
            "${source}/soc/stm32/stm32f4/private/rcc_hal")
        target_include_directories("${vendor}" SYSTEM PRIVATE
            "${source}/vendors/st/cmsis_device_f4/Include")
        include("${source}/cmake/platform/CMSISCore.cmake")
        nexus_use_cmsis_core("${vendor}")
        if(COMMAND nexus_use_cmsis_host_model AND NOT CMAKE_CROSSCOMPILING)
            nexus_use_cmsis_host_model("${vendor}")
        endif()
        foreach(policy nexus_build_options Nexus::Arch)
            if(TARGET "${policy}")
                target_link_libraries("${vendor}" PRIVATE "${policy}")
            endif()
        endforeach()
        target_compile_options("${vendor}" PRIVATE
            -ffunction-sections -fdata-sections)
        target_sources("${target}" PRIVATE "$<TARGET_OBJECTS:${vendor}>")
    endif()
    if(ARGC EQUAL 2)
        get_target_property(extended "${vendor}" NEXUS_STM32F4_RCC_EXTENDED)
        if(NOT extended)
            target_sources("${vendor}" PRIVATE
                "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_rcc_ex.c")
            set_target_properties("${vendor}" PROPERTIES
                NEXUS_STM32F4_RCC_EXTENDED TRUE)
        endif()
    endif()
endfunction()

# Only the selected F407 finite TX mode owns this UART runtime. Insert the
# objects in the consumer archive, avoiding a second transport or link cycle.
function(nexus_stm32f407_uart_hal target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "STM32F407 UART HAL requires one owned target")
    endif()
    nexus_stm32f4_device_definition("${target}" device)
    nexus_stm32f4_official("${target}")
    set(vendor "${target}_stm32f407_hal")
    if(TARGET "${vendor}")
        message(FATAL_ERROR "STM32F407 UART HAL transport already selected")
    endif()
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(private "${source}/soc/stm32/stm32f4/private")
    add_library("${vendor}" OBJECT
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_uart.c")
    nexus_stm32f4_dma_hal("${target}")
    nexus_stm32f4_rcc_hal("${target}")
    nexus_stm32f4_official("${vendor}")
    target_compile_features("${vendor}" PRIVATE c_std_11)
    set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions("${vendor}" PRIVATE "${device}")
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

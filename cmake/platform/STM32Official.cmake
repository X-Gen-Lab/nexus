# Reviewed inline LL only: no HAL_Init, tick, mutable Handle or vendor runtime.
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

# One selected H743 link gate; CMake remains the production source graph.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ControllerModes.cmake")

function(nexus_h743_flash_ram_audit target source config family)
    if(NOT family STREQUAL "stm32h743")
        return()
    endif()
    set(NEXUS_CONFIG_DIR "${config}")
    nexus_controller_modes(flash_modes flash)
    if(NOT flash_modes)
        return()
    endif()
    get_target_property(stack_files nexus_soc_stm32h743 NEXUS_H743_FLASH_SU_FILES)
    if(NOT stack_files OR stack_files MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "Selected H743 Flash has no declared stack outputs")
    endif()
    if(NOT CMAKE_OBJDUMP)
        message(FATAL_ERROR "Selected H743 Flash needs the actual toolchain objdump")
    endif()
    set(report "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.h743-flash-ram.json")
    set(base "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.h743-flash-ram")
    set(checker_files
        tools/measurement/h743_flash_ram.py
        tools/measurement/flash_execution.py
        tools/measurement/flash_ram_static.py
        tools/measurement/soc_costs.py
        tools/measurement/static_storage.py
        tools/measurement/resources.py tools/measurement/elf.py
        tools/evidence/common.py tools/evidence/identity.py
        tools/configure/memory.py)
    set(checker_paths)
    foreach(file IN LISTS checker_files)
        list(APPEND checker_paths "${source}/${file}")
    endforeach()
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
        ${checker_paths} ${stack_files} "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${config}/resolved.json" "${config}/input_paths.json")
    add_custom_command(TARGET ${target} POST_BUILD
        BYPRODUCTS "${report}" "${base}.objdump.raw"
            "${base}.objdump.command.json" "${base}.compiler.raw"
            "${base}.compiler.command.json"
        COMMAND "${Python3_EXECUTABLE}" -B
            "${source}/tools/measurement/h743_flash_ram.py"
            --elf "$<TARGET_FILE:${target}>" --resolved "${config}/resolved.json"
            --compile-commands "${CMAKE_BINARY_DIR}/compile_commands.json"
            --objdump "${CMAKE_OBJDUMP}" --report "${report}"
        VERBATIM)
endfunction()

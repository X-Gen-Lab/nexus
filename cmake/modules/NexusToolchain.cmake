#-----------------------------------------------------------------------------
# NexusToolchain.cmake - Unified Toolchain Management
#-----------------------------------------------------------------------------
# NexusToolchain.cmake
# Unified toolchain detection, configuration, and management
# Author: Nexus Team
#
# This module provides:
# - Automatic toolchain detection
# - Unified toolchain configuration interface
# - Toolchain-agnostic helper functions
# - Consistent binary generation across toolchains
#
#-----------------------------------------------------------------------------

include_guard(GLOBAL)

#-----------------------------------------------------------------------------
# Toolchain Registry
#-----------------------------------------------------------------------------

# Define all supported toolchains with metadata
set(_NEXUS_TOOLCHAIN_REGISTRY "")

#
# Register a toolchain
# Arguments:
#   NAME: Toolchain identifier
#   DESCRIPTION: Human-readable description
#   FAMILY: Toolchain family (gcc, clang, iar)
#   VENDOR: Toolchain vendor
#   COMPILER_ID: CMake compiler ID pattern
#   PROGRAMS: List of required programs
#
macro(nexus_register_toolchain)
    cmake_parse_arguments(
        _ARG
        ""
        "NAME;DESCRIPTION;FAMILY;VENDOR;COMPILER_ID"
        "PROGRAMS"
        ${ARGN}
    )

    set(_ENTRY "")
    list(APPEND _ENTRY "name=${_ARG_NAME}")
    list(APPEND _ENTRY "description=${_ARG_DESCRIPTION}")
    list(APPEND _ENTRY "family=${_ARG_FAMILY}")
    list(APPEND _ENTRY "vendor=${_ARG_VENDOR}")
    list(APPEND _ENTRY "compiler_id=${_ARG_COMPILER_ID}")

    if(_ARG_PROGRAMS)
        string(REPLACE ";" "," _PROGRAMS_STR "${_ARG_PROGRAMS}")
        list(APPEND _ENTRY "programs=${_PROGRAMS_STR}")
    endif()

    string(REPLACE ";" "|" _ENTRY_STR "${_ENTRY}")
    list(APPEND _NEXUS_TOOLCHAIN_REGISTRY "${_ENTRY_STR}")

    message(VERBOSE "Registered toolchain: ${_ARG_NAME}")
endmacro()

# Register supported toolchains
nexus_register_toolchain(
    NAME "arm-none-eabi-gcc"
    DESCRIPTION "ARM GCC (Free, Open Source)"
    FAMILY "gcc"
    VENDOR "ARM"
    COMPILER_ID "GNU"
    PROGRAMS "arm-none-eabi-gcc;arm-none-eabi-g++;arm-none-eabi-as;arm-none-eabi-ar;arm-none-eabi-objcopy;arm-none-eabi-size"
)

nexus_register_toolchain(
    NAME "armclang"
    DESCRIPTION "ARM Clang (Commercial)"
    FAMILY "clang"
    VENDOR "ARM"
    COMPILER_ID "ARMClang"
    PROGRAMS "armclang;armasm;armlink;armar;fromelf"
)

nexus_register_toolchain(
    NAME "iar-arm"
    DESCRIPTION "IAR Embedded Workbench for ARM (Commercial)"
    FAMILY "iar"
    VENDOR "IAR"
    COMPILER_ID "IAR"
    PROGRAMS "iccarm;iasmarm;ilinkarm;iarchive;ielftool"
)

nexus_register_toolchain(
    NAME "gcc"
    DESCRIPTION "GNU Compiler Collection (Native)"
    FAMILY "gcc"
    VENDOR "GNU"
    COMPILER_ID "GNU"
    PROGRAMS "gcc;g++;as;ar"
)

nexus_register_toolchain(
    NAME "clang"
    DESCRIPTION "LLVM Clang (Native)"
    FAMILY "clang"
    VENDOR "LLVM"
    COMPILER_ID "Clang"
    PROGRAMS "clang;clang++;llvm-ar"
)

nexus_register_toolchain(
    NAME "msvc"
    DESCRIPTION "Microsoft Visual C++ (Native)"
    FAMILY "msvc"
    VENDOR "Microsoft"
    COMPILER_ID "MSVC"
    PROGRAMS "cl;link;lib"
)

#-----------------------------------------------------------------------------
# Toolchain Detection
#-----------------------------------------------------------------------------

#
# Detect available toolchains on the system
# Arguments:
#   OUTPUT_VAR: Output variable (list of available toolchain names)
#   PLATFORM: Target platform (optional, filters by platform)
#
function(nexus_detect_toolchains OUTPUT_VAR)
    cmake_parse_arguments(ARG "" "PLATFORM" "" ${ARGN})

    set(AVAILABLE_TOOLCHAINS "")

    foreach(ENTRY ${_NEXUS_TOOLCHAIN_REGISTRY})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        # Extract toolchain info
        foreach(FIELD ${ENTRY_LIST})
            if(FIELD MATCHES "^name=(.+)$")
                set(TC_NAME ${CMAKE_MATCH_1})
            elseif(FIELD MATCHES "^programs=(.+)$")
                set(TC_PROGRAMS ${CMAKE_MATCH_1})
            endif()
        endforeach()

        # Check if all required programs are available
        if(TC_PROGRAMS)
            string(REPLACE "," ";" PROGRAM_LIST "${TC_PROGRAMS}")
            set(ALL_FOUND TRUE)

            foreach(PROGRAM ${PROGRAM_LIST})
                find_program(_PROG_${PROGRAM} ${PROGRAM})
                if(NOT _PROG_${PROGRAM})
                    set(ALL_FOUND FALSE)
                    break()
                endif()
            endforeach()

            if(ALL_FOUND)
                list(APPEND AVAILABLE_TOOLCHAINS ${TC_NAME})
                message(VERBOSE "Found toolchain: ${TC_NAME}")
            endif()
        endif()
    endforeach()

    set(${OUTPUT_VAR} ${AVAILABLE_TOOLCHAINS} PARENT_SCOPE)
endfunction()

#
# Get current toolchain information
# Arguments:
#   OUTPUT_VAR: Output variable (toolchain name)
#
function(nexus_get_current_toolchain OUTPUT_VAR)
    # Check compiler path for ARM toolchains
    if(CMAKE_C_COMPILER MATCHES "arm-none-eabi")
        set(${OUTPUT_VAR} "arm-none-eabi-gcc" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "ARMClang")
        set(${OUTPUT_VAR} "armclang" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "IAR")
        set(${OUTPUT_VAR} "iar-arm" PARENT_SCOPE)
        return()
    endif()

    # Try to match compiler ID with registered toolchains
    foreach(ENTRY ${_NEXUS_TOOLCHAIN_REGISTRY})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        foreach(FIELD ${ENTRY_LIST})
            if(FIELD MATCHES "^name=(.+)$")
                set(TC_NAME ${CMAKE_MATCH_1})
            elseif(FIELD MATCHES "^compiler_id=(.+)$")
                set(TC_COMPILER_ID ${CMAKE_MATCH_1})
            endif()
        endforeach()

        if(CMAKE_C_COMPILER_ID MATCHES "${TC_COMPILER_ID}")
            set(${OUTPUT_VAR} ${TC_NAME} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Fallback to compiler ID
    set(${OUTPUT_VAR} ${CMAKE_C_COMPILER_ID} PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------------
# Unified Binary Generation
#-----------------------------------------------------------------------------

#
# Generate binary files from ELF (toolchain-agnostic)
# Arguments:
#   TARGET: Target name
#   FORMATS: Output formats (BIN, HEX, LST, MAP)
#
function(nexus_generate_outputs TARGET)
    cmake_parse_arguments(ARG "" "" "FORMATS" ${ARGN})

    if(NOT ARG_FORMATS)
        set(ARG_FORMATS "BIN;HEX")
    endif()

    # Detect toolchain
    nexus_get_current_toolchain(TOOLCHAIN)

    # Generate outputs based on toolchain (case-insensitive matching)
    string(TOLOWER "${TOOLCHAIN}" TOOLCHAIN_LOWER)
    if(TOOLCHAIN_LOWER MATCHES "arm.*gcc|gnu.*arm")
        _nexus_generate_outputs_gcc(${TARGET} "${ARG_FORMATS}")
    elseif(TOOLCHAIN_LOWER MATCHES "armclang")
        _nexus_generate_outputs_armclang(${TARGET} "${ARG_FORMATS}")
    elseif(TOOLCHAIN_LOWER MATCHES "iar")
        _nexus_generate_outputs_iar(${TARGET} "${ARG_FORMATS}")
    else()
        message(WARNING "Unknown toolchain for binary generation: ${TOOLCHAIN}")
    endif()
endfunction()

#
# GCC binary generation
#
function(_nexus_generate_outputs_gcc TARGET FORMATS)
    find_program(OBJCOPY arm-none-eabi-objcopy)
    find_program(OBJDUMP arm-none-eabi-objdump)
    find_program(SIZE arm-none-eabi-size)

    # Get the directory where the ELF file will be placed
    get_target_property(TARGET_OUTPUT_DIR ${TARGET} RUNTIME_OUTPUT_DIRECTORY)
    if(NOT TARGET_OUTPUT_DIR)
        set(TARGET_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    if("BIN" IN_LIST FORMATS AND OBJCOPY)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${OBJCOPY} -O binary $<TARGET_FILE:${TARGET}> ${TARGET}.bin
            WORKING_DIRECTORY ${TARGET_OUTPUT_DIR}
            COMMENT "Generating ${TARGET}.bin"
            BYPRODUCTS ${TARGET_OUTPUT_DIR}/${TARGET}.bin
            VERBATIM
        )
    endif()

    if("HEX" IN_LIST FORMATS AND OBJCOPY)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${OBJCOPY} -O ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
            WORKING_DIRECTORY ${TARGET_OUTPUT_DIR}
            COMMENT "Generating ${TARGET}.hex"
            BYPRODUCTS ${TARGET_OUTPUT_DIR}/${TARGET}.hex
            VERBATIM
        )
    endif()

    if("LST" IN_LIST FORMATS AND OBJDUMP)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${OBJDUMP} -h -S $<TARGET_FILE:${TARGET}> > ${TARGET}.lst
            WORKING_DIRECTORY ${TARGET_OUTPUT_DIR}
            COMMENT "Generating ${TARGET}.lst"
            BYPRODUCTS ${TARGET_OUTPUT_DIR}/${TARGET}.lst
            VERBATIM
        )
    endif()

    if(SIZE)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${SIZE} $<TARGET_FILE:${TARGET}>
            COMMENT "Program Size:"
            VERBATIM
        )
    endif()
endfunction()

#
# ARM Clang binary generation
#
function(_nexus_generate_outputs_armclang TARGET FORMATS)
    # Try to find fromelf in the same directory as armlink
    if(CMAKE_LINKER)
        get_filename_component(ARMCLANG_BIN_DIR "${CMAKE_LINKER}" DIRECTORY)
        find_program(FROMELF fromelf HINTS "${ARMCLANG_BIN_DIR}" NO_DEFAULT_PATH)
    endif()

    # Fallback to system PATH
    if(NOT FROMELF)
        find_program(FROMELF fromelf)
    endif()

    if(NOT FROMELF)
        message(WARNING "fromelf not found, cannot generate binary outputs")
        return()
    endif()

    # Get the directory where the ELF file will be placed
    get_target_property(TARGET_OUTPUT_DIR ${TARGET} RUNTIME_OUTPUT_DIRECTORY)
    if(NOT TARGET_OUTPUT_DIR)
        set(TARGET_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    if("BIN" IN_LIST FORMATS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${FROMELF} --bin --output ${TARGET}.bin $<TARGET_FILE:${TARGET}>
            WORKING_DIRECTORY ${TARGET_OUTPUT_DIR}
            COMMENT "Generating ${TARGET}.bin"
            BYPRODUCTS ${TARGET_OUTPUT_DIR}/${TARGET}.bin
            VERBATIM
        )
    endif()

    if("HEX" IN_LIST FORMATS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${FROMELF} --i32 --output ${TARGET}.hex $<TARGET_FILE:${TARGET}>
            WORKING_DIRECTORY ${TARGET_OUTPUT_DIR}
            COMMENT "Generating ${TARGET}.hex"
            BYPRODUCTS ${TARGET_OUTPUT_DIR}/${TARGET}.hex
            VERBATIM
        )
    endif()

    if("LST" IN_LIST FORMATS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${FROMELF} --text -c --output ${TARGET}.lst $<TARGET_FILE:${TARGET}>
            WORKING_DIRECTORY ${TARGET_OUTPUT_DIR}
            COMMENT "Generating ${TARGET}.lst"
            BYPRODUCTS ${TARGET_OUTPUT_DIR}/${TARGET}.lst
            VERBATIM
        )
    endif()

    # ARM Clang prints size info during linking
endfunction()

#
# IAR binary generation
#
function(_nexus_generate_outputs_iar TARGET FORMATS)
    find_program(IELFTOOL ielftool)

    if(NOT IELFTOOL)
        message(WARNING "ielftool not found, cannot generate binary outputs")
        return()
    endif()

    if("BIN" IN_LIST FORMATS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${IELFTOOL} --bin $<TARGET_FILE:${TARGET}> ${TARGET}.bin
            COMMENT "Generating ${TARGET}.bin"
            VERBATIM
        )
    endif()

    if("HEX" IN_LIST FORMATS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${IELFTOOL} --ihex $<TARGET_FILE:${TARGET}> ${TARGET}.hex
            COMMENT "Generating ${TARGET}.hex"
            VERBATIM
        )
    endif()

    if("LST" IN_LIST FORMATS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${IELFTOOL} --code $<TARGET_FILE:${TARGET}> ${TARGET}.lst
            COMMENT "Generating ${TARGET}.lst"
            VERBATIM
        )
    endif()
endfunction()

#-----------------------------------------------------------------------------
# Unified Target Configuration
#-----------------------------------------------------------------------------

#
# Configure ARM target (toolchain-agnostic)
# Arguments:
#   TARGET: Target name
#   LINKER_SCRIPT: Path to linker script
#   GENERATE: Output formats to generate (BIN, HEX, LST)
#
function(nexus_configure_arm_target TARGET)
    cmake_parse_arguments(ARG "" "LINKER_SCRIPT" "GENERATE" ${ARGN})

    if(NOT ARG_GENERATE)
        set(ARG_GENERATE "BIN;HEX")
    endif()

    # Detect toolchain
    nexus_get_current_toolchain(TOOLCHAIN)

    # Configure linker script
    if(ARG_LINKER_SCRIPT)
        if(TOOLCHAIN MATCHES "arm-none-eabi-gcc")
            target_link_options(${TARGET} PRIVATE -T${ARG_LINKER_SCRIPT})
        elseif(TOOLCHAIN MATCHES "armclang")
            target_link_options(${TARGET} PRIVATE --scatter=${ARG_LINKER_SCRIPT})
        elseif(TOOLCHAIN MATCHES "iar-arm")
            target_link_options(${TARGET} PRIVATE --config ${ARG_LINKER_SCRIPT})
        endif()
    endif()

    # Add standard libraries (GCC only)
    if(TOOLCHAIN MATCHES "arm-none-eabi-gcc")
        target_link_libraries(${TARGET} PRIVATE c m nosys)
    endif()

    # Generate binary outputs
    nexus_generate_outputs(${TARGET} FORMATS ${ARG_GENERATE})
endfunction()

#-----------------------------------------------------------------------------
# Toolchain Information
#-----------------------------------------------------------------------------

#
# Print toolchain registry
#
function(nexus_print_toolchain_registry)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Toolchain Registry")
    message(STATUS "========================================")

    foreach(ENTRY ${_NEXUS_TOOLCHAIN_REGISTRY})
        string(REPLACE "|" ";" ENTRY_LIST "${ENTRY}")

        foreach(FIELD ${ENTRY_LIST})
            if(FIELD MATCHES "^name=(.+)$")
                set(TC_NAME ${CMAKE_MATCH_1})
            elseif(FIELD MATCHES "^description=(.+)$")
                set(TC_DESC ${CMAKE_MATCH_1})
            elseif(FIELD MATCHES "^family=(.+)$")
                set(TC_FAMILY ${CMAKE_MATCH_1})
            endif()
        endforeach()

        message(STATUS "")
        message(STATUS "  ${TC_NAME}")
        message(STATUS "    Description: ${TC_DESC}")
        message(STATUS "    Family:      ${TC_FAMILY}")
    endforeach()

    message(STATUS "")
    message(STATUS "========================================")
endfunction()

#
# Print available toolchains
#
function(nexus_print_available_toolchains)
    nexus_detect_toolchains(AVAILABLE)

    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Available Toolchains")
    message(STATUS "========================================")

    if(AVAILABLE)
        foreach(TC ${AVAILABLE})
            message(STATUS "  ✓ ${TC}")
        endforeach()
    else()
        message(STATUS "  (none detected)")
    endif()

    message(STATUS "========================================")
endfunction()

message(STATUS "NexusToolchain module loaded")

#-----------------------------------------------------------------------------
# End of NexusToolchain.cmake
#-----------------------------------------------------------------------------

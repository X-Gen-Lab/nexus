##############################################################################
# NexusCompilerConfig.cmake - Compiler Configuration Application Module
##############################################################################
#
# NexusCompilerConfig.cmake
# Apply Kconfig compiler and linker configurations to CMake
# Author: Nexus Team
#
# This module bridges Kconfig configuration and actual compiler/linker flags.
# It reads CONFIG_COMPILER_* and CONFIG_LINKER_* variables from Kconfig
# and applies them as compiler/linker options.
#
# Functions:
# - nexus_apply_compiler_kconfig()  - Apply all compiler configurations
# - nexus_apply_linker_kconfig()    - Apply all linker configurations
#
##############################################################################

include_guard(GLOBAL)

# Load helper functions
include(NexusHelpers)

##############################################################################
# Main Entry Points
##############################################################################

#
# Apply all Kconfig compiler configurations
#
function(nexus_apply_compiler_kconfig)
    # Silently apply all compiler configurations
    # Only show summary at the end

    # 1. Character type configuration
    _nexus_apply_char_config()

    # 2. Code generation options
    _nexus_apply_codegen_config()

    # 3. Warning level configuration
    _nexus_apply_warning_config()

    # 4. Debug information configuration
    _nexus_apply_debug_config()

    # 5. Optimization configuration
    _nexus_apply_optimization_config()

    # 6. Standard library configuration
    _nexus_apply_stdlib_config()

    # 7. ARM-specific configuration
    if(NOT PLATFORM_NATIVE)
        _nexus_apply_arm_config()
    endif()

    # 8. Advanced options
    _nexus_apply_advanced_config()

    # 9. Custom flags
    _nexus_apply_custom_compiler_flags()
endfunction()

#
# Apply all Kconfig linker configurations
#
function(nexus_apply_linker_kconfig)
    # Silently apply all linker configurations
    # Only show summary at the end

    # 1. Memory configuration
    _nexus_apply_memory_config()

    # 2. Linker optimization
    _nexus_apply_linker_optimization()

    # 3. Linker output options
    _nexus_apply_linker_output()

    # 4. Custom linker flags
    _nexus_apply_custom_linker_flags()
endfunction()

##############################################################################
# Character Type Configuration
##############################################################################

#
# Apply character type configuration
#
function(_nexus_apply_char_config)
    # Unsigned char by default
    if(DEFINED CONFIG_COMPILER_UNSIGNED_CHAR AND CONFIG_COMPILER_UNSIGNED_CHAR)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG)
            add_compile_options(-funsigned-char)
            nexus_log(VERBOSE "  Applied: -funsigned-char")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/J)
            nexus_log(VERBOSE "  Applied: /J (unsigned char)")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-funsigned-char)
            nexus_log(VERBOSE "  Applied: -funsigned-char")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--char_is_unsigned)
            nexus_log(VERBOSE "  Applied: --char_is_unsigned")
        endif()
    endif()

    # Short enums
    if(DEFINED CONFIG_COMPILER_SHORT_ENUMS AND CONFIG_COMPILER_SHORT_ENUMS)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-fshort-enums)
            nexus_log(VERBOSE "  Applied: -fshort-enums")
        elseif(NEXUS_COMPILER_MSVC)
            nexus_log(VERBOSE "  Short enums not supported on MSVC")
        elseif(NEXUS_COMPILER_IAR)
            # IAR uses --enum_is_int to force int (opposite behavior)
            nexus_log(VERBOSE "  Short enums: IAR default behavior")
        endif()
    endif()

    # Short wchar_t
    if(DEFINED CONFIG_COMPILER_SHORT_WCHAR AND CONFIG_COMPILER_SHORT_WCHAR)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-fshort-wchar)
            nexus_log(VERBOSE "  Applied: -fshort-wchar")
        elseif(NEXUS_COMPILER_MSVC)
            nexus_log(VERBOSE "  Short wchar_t: MSVC default is 16-bit")
        elseif(NEXUS_COMPILER_IAR)
            nexus_log(VERBOSE "  Short wchar_t not supported on IAR")
        endif()
    endif()
endfunction()

##############################################################################
# Code Generation Options
##############################################################################

#
# Apply code generation configuration
#
function(_nexus_apply_codegen_config)
    # Disable builtin functions
    if(DEFINED CONFIG_COMPILER_NO_BUILTIN AND CONFIG_COMPILER_NO_BUILTIN)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-fno-builtin)
            nexus_log(VERBOSE "  Applied: -fno-builtin")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/Oi-)
            nexus_log(VERBOSE "  Applied: /Oi- (disable intrinsics)")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--no_builtin)
            nexus_log(VERBOSE "  Applied: --no_builtin")
        endif()
    endif()

    # Function sections (for garbage collection)
    if(DEFINED CONFIG_COMPILER_FUNCTION_SECTIONS AND CONFIG_COMPILER_FUNCTION_SECTIONS)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-ffunction-sections -fdata-sections)
            nexus_log(VERBOSE "  Applied: -ffunction-sections -fdata-sections")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/Gy)
            nexus_log(VERBOSE "  Applied: /Gy (function-level linking)")
        elseif(NEXUS_COMPILER_IAR)
            nexus_log(VERBOSE "  Function sections: IAR automatic")
        endif()
    endif()

    # Disable common blocks
    if(DEFINED CONFIG_COMPILER_NO_COMMON AND CONFIG_COMPILER_NO_COMMON)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-fno-common)
            nexus_log(VERBOSE "  Applied: -fno-common")
        endif()
    endif()

    # Stack protector (native platform only)
    if(DEFINED CONFIG_COMPILER_STACK_PROTECTOR AND CONFIG_COMPILER_STACK_PROTECTOR)
        if(PLATFORM_NATIVE)
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG)
                add_compile_options(-fstack-protector-strong)
                nexus_log(VERBOSE "  Applied: -fstack-protector-strong")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/GS)
                nexus_log(VERBOSE "  Applied: /GS (stack protection)")
            endif()
        endif()
    endif()

    # Omit frame pointer (not in Debug builds)
    if(DEFINED CONFIG_COMPILER_OMIT_FRAME_POINTER AND CONFIG_COMPILER_OMIT_FRAME_POINTER)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-fomit-frame-pointer)
                nexus_log(VERBOSE "  Applied: -fomit-frame-pointer")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/Oy)
                nexus_log(VERBOSE "  Applied: /Oy (omit frame pointer)")
            endif()
        endif()
    endif()
endfunction()

##############################################################################
# Warning Level Configuration
##############################################################################


#
# Apply warning level configuration
#
function(_nexus_apply_warning_config)
    if(NOT DEFINED CONFIG_COMPILER_WARNING_LEVEL)
        return()
    endif()

    set(WARNING_LEVEL ${CONFIG_COMPILER_WARNING_LEVEL})

    if(WARNING_LEVEL STREQUAL "NONE")
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-w)
            nexus_log(VERBOSE "  Applied: -w (no warnings)")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/w)
            nexus_log(VERBOSE "  Applied: /w (no warnings)")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--no_warnings)
            nexus_log(VERBOSE "  Applied: --no_warnings")
        endif()

    elseif(WARNING_LEVEL STREQUAL "BASIC")
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-Wall)
            nexus_log(VERBOSE "  Applied: -Wall")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/W3)
            nexus_log(VERBOSE "  Applied: /W3")
        endif()

    elseif(WARNING_LEVEL STREQUAL "STANDARD")
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-Wall -Wextra)
            nexus_log(VERBOSE "  Applied: -Wall -Wextra")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/W4)
            nexus_log(VERBOSE "  Applied: /W4")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--remarks)
            nexus_log(VERBOSE "  Applied: --remarks")
        endif()

    elseif(WARNING_LEVEL STREQUAL "EXTRA")
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-Wall -Wextra -Wshadow -Wconversion)
            nexus_log(VERBOSE "  Applied: -Wall -Wextra -Wshadow -Wconversion")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/W4 /Wall)
            nexus_log(VERBOSE "  Applied: /W4 /Wall")
        endif()

    elseif(WARNING_LEVEL STREQUAL "PEDANTIC")
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
            nexus_log(VERBOSE "  Applied: -Wall -Wextra -Wpedantic -Wshadow -Wconversion")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/Wall /permissive-)
            nexus_log(VERBOSE "  Applied: /Wall /permissive-")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--strict)
            nexus_log(VERBOSE "  Applied: --strict")
        endif()
    endif()

    # Warnings as errors
    if(DEFINED CONFIG_COMPILER_WARNINGS_AS_ERRORS AND CONFIG_COMPILER_WARNINGS_AS_ERRORS)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-Werror)
            nexus_log(VERBOSE "  Applied: -Werror")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/WX)
            nexus_log(VERBOSE "  Applied: /WX")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--warnings_are_errors)
            nexus_log(VERBOSE "  Applied: --warnings_are_errors")
        endif()
    endif()
endfunction()

##############################################################################
# Debug Information Configuration
##############################################################################

#
# Apply debug information configuration
#
function(_nexus_apply_debug_config)
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        return()
    endif()

    # Debug format
    if(DEFINED CONFIG_COMPILER_DEBUG_FORMAT)
        set(DEBUG_FORMAT ${CONFIG_COMPILER_DEBUG_FORMAT})

        if(DEBUG_FORMAT STREQUAL "DWARF2")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-gdwarf-2)
                nexus_log(VERBOSE "  Applied: -gdwarf-2")
            endif()

        elseif(DEBUG_FORMAT STREQUAL "DWARF3")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-gdwarf-3)
                nexus_log(VERBOSE "  Applied: -gdwarf-3")
            endif()

        elseif(DEBUG_FORMAT STREQUAL "DWARF4")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-gdwarf-4)
                nexus_log(VERBOSE "  Applied: -gdwarf-4")
            endif()

        elseif(DEBUG_FORMAT STREQUAL "DWARF5")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-gdwarf-5)
                nexus_log(VERBOSE "  Applied: -gdwarf-5")
            endif()

        elseif(DEBUG_FORMAT STREQUAL "PDB")
            if(NEXUS_COMPILER_MSVC)
                add_compile_options(/Zi)
                nexus_log(VERBOSE "  Applied: /Zi (PDB)")
            endif()
        endif()
    endif()

    # Debug level
    if(DEFINED CONFIG_COMPILER_DEBUG_LEVEL)
        set(DEBUG_LEVEL ${CONFIG_COMPILER_DEBUG_LEVEL})

        if(DEBUG_LEVEL EQUAL 0)
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-g0)
                nexus_log(VERBOSE "  Applied: -g0 (no debug info)")
            endif()

        elseif(DEBUG_LEVEL EQUAL 1)
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-g1)
                nexus_log(VERBOSE "  Applied: -g1 (minimal debug info)")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/Z7)
                nexus_log(VERBOSE "  Applied: /Z7 (minimal debug info)")
            endif()

        elseif(DEBUG_LEVEL EQUAL 2)
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-g)
                nexus_log(VERBOSE "  Applied: -g (default debug info)")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/Zi)
                nexus_log(VERBOSE "  Applied: /Zi (full debug info)")
            endif()

        elseif(DEBUG_LEVEL EQUAL 3)
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-g3)
                nexus_log(VERBOSE "  Applied: -g3 (maximum debug info)")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/Zi)
                nexus_log(VERBOSE "  Applied: /Zi (full debug info)")
            endif()
        endif()
    endif()
endfunction()

##############################################################################
# Optimization Configuration
##############################################################################

#
# Apply optimization configuration
#
function(_nexus_apply_optimization_config)
    # Link Time Optimization (LTO)
    if(DEFINED CONFIG_COMPILER_LTO AND CONFIG_COMPILER_LTO)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG)
                add_compile_options(-flto)
                add_link_options(-flto)
                nexus_log(VERBOSE "  Applied: -flto (Link Time Optimization)")
            elseif(NEXUS_COMPILER_ARM_CLANG)
                # ARM Clang: compiler uses -flto, linker uses --lto
                add_compile_options(-flto)
                add_link_options(--lto)
                nexus_log(VERBOSE "  Applied: -flto / --lto (Link Time Optimization)")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/GL)
                add_link_options(/LTCG)
                nexus_log(VERBOSE "  Applied: /GL /LTCG (Link Time Code Generation)")
            elseif(NEXUS_COMPILER_IAR)
                add_compile_options(--lto)
                nexus_log(VERBOSE "  Applied: --lto")
            endif()
        endif()
    endif()

    # Optimize for size
    if(DEFINED CONFIG_COMPILER_OPTIMIZE_SIZE AND CONFIG_COMPILER_OPTIMIZE_SIZE)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
                add_compile_options(-Os)
                nexus_log(VERBOSE "  Applied: -Os (optimize for size)")
            elseif(NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
                   NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-Oz)
                nexus_log(VERBOSE "  Applied: -Oz (aggressive size optimization)")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/O1 /Os)
                nexus_log(VERBOSE "  Applied: /O1 /Os (favor size)")
            elseif(NEXUS_COMPILER_IAR)
                add_compile_options(-Ohz)
                nexus_log(VERBOSE "  Applied: -Ohz (high size optimization)")
            endif()
        endif()
    endif()

    # Fast math
    if(DEFINED CONFIG_COMPILER_FAST_MATH AND CONFIG_COMPILER_FAST_MATH)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-ffast-math)
                nexus_log(VERBOSE "  Applied: -ffast-math")
            elseif(NEXUS_COMPILER_MSVC)
                add_compile_options(/fp:fast)
                nexus_log(VERBOSE "  Applied: /fp:fast")
            elseif(NEXUS_COMPILER_IAR)
                add_compile_options(--relaxed_fp)
                nexus_log(VERBOSE "  Applied: --relaxed_fp")
            endif()
        endif()
    endif()

    # Loop unrolling
    if(DEFINED CONFIG_COMPILER_UNROLL_LOOPS AND CONFIG_COMPILER_UNROLL_LOOPS)
        if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
            if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
               NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
                add_compile_options(-funroll-loops)
                nexus_log(VERBOSE "  Applied: -funroll-loops")
            elseif(NEXUS_COMPILER_MSVC)
                nexus_log(VERBOSE "  Loop unrolling: MSVC automatic in /O2")
            elseif(NEXUS_COMPILER_IAR)
                nexus_log(VERBOSE "  Loop unrolling: IAR automatic in high optimization")
            endif()
        endif()
    endif()
endfunction()

##############################################################################
# Standard Library Configuration
##############################################################################

#
# Apply standard library configuration
#
function(_nexus_apply_stdlib_config)
    # ARM Clang microlib
    if(DEFINED CONFIG_COMPILER_USE_MICROLIB AND CONFIG_COMPILER_USE_MICROLIB)
        if(NEXUS_COMPILER_ARM_CLANG)
            add_link_options(--library_type=microlib)
            nexus_log(VERBOSE "  Applied: --library_type=microlib")

            # Add assembler macro definition for MicroLIB
            # Note: Using SHELL: prefix to pass the --pd flag with spaces correctly
            set(_microlib_asm_flag "SHELL:$<$<COMPILE_LANGUAGE:ASM>:--pd \"__MICROLIB SETA 1\">")
            add_compile_options(${_microlib_asm_flag})
            nexus_log(VERBOSE "  Applied: __MICROLIB SETA 1 (assembler)")
        endif()
    else()
        # Explicitly disable MicroLIB for assembler
        if(NEXUS_COMPILER_ARM_CLANG)
            set(_microlib_asm_flag "SHELL:$<$<COMPILE_LANGUAGE:ASM>:--pd \"__MICROLIB SETA 0\">")
            add_compile_options(${_microlib_asm_flag})
            nexus_log(VERBOSE "  Applied: __MICROLIB SETA 0 (assembler)")
        endif()
    endif()

    # ARM GCC newlib-nano
    if(DEFINED CONFIG_COMPILER_USE_NANO AND CONFIG_COMPILER_USE_NANO)
        if(NEXUS_COMPILER_ARM_GCC)
            add_link_options(--specs=nano.specs)
            nexus_log(VERBOSE "  Applied: --specs=nano.specs")

            # Enable printf float support if configured
            if(DEFINED CONFIG_COMPILER_PRINTF_FLOAT AND CONFIG_COMPILER_PRINTF_FLOAT)
                add_link_options(-u _printf_float -u _scanf_float)
                nexus_log(VERBOSE "  Applied: -u _printf_float -u _scanf_float")
            endif()
        endif()
    endif()

    # ARM GCC nosys specs
    if(DEFINED CONFIG_COMPILER_USE_NOSYS AND CONFIG_COMPILER_USE_NOSYS)
        if(NEXUS_COMPILER_ARM_GCC)
            add_link_options(--specs=nosys.specs)
            nexus_log(VERBOSE "  Applied: --specs=nosys.specs")
        endif()
    endif()
endfunction()

##############################################################################
# ARM-Specific Configuration
##############################################################################

#
# Apply ARM-specific configuration
#
function(_nexus_apply_arm_config)
    # ARM/Thumb interworking
    if(DEFINED CONFIG_ARM_APCS_INTERWORK AND CONFIG_ARM_APCS_INTERWORK)
        if(NEXUS_COMPILER_ARM_GCC)
            add_compile_options(-mthumb-interwork)
            nexus_log(VERBOSE "  Applied: -mthumb-interwork")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            # ARM Clang uses assembler flag for APCS interwork
            add_compile_options($<$<COMPILE_LANGUAGE:ASM>:--apcs=interwork>)
            nexus_log(VERBOSE "  Applied: --apcs=interwork (assembler)")
        endif()
    endif()

    # Read-Only Position Independent (ROPI)
    if(DEFINED CONFIG_ARM_ROPI AND CONFIG_ARM_ROPI)
        if(NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-fropi)
            nexus_log(VERBOSE "  Applied: -fropi")
        endif()
    endif()

    # Read-Write Position Independent (RWPI)
    if(DEFINED CONFIG_ARM_RWPI AND CONFIG_ARM_RWPI)
        if(NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-frwpi)
            nexus_log(VERBOSE "  Applied: -frwpi")
        endif()
    endif()

    # Split LDM/STM instructions
    if(DEFINED CONFIG_ARM_SPLIT_LDMSTM AND CONFIG_ARM_SPLIT_LDMSTM)
        if(NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-mno-unaligned-access)
            nexus_log(VERBOSE "  Applied: -mno-unaligned-access")
        endif()
    endif()

    # Force Thumb mode
    if(DEFINED CONFIG_ARM_THUMB_MODE AND CONFIG_ARM_THUMB_MODE)
        if(NEXUS_COMPILER_ARM_GCC)
            add_compile_options(-mthumb)
            nexus_log(VERBOSE "  Applied: -mthumb")
        endif()
    endif()

    # Unaligned access support
    if(DEFINED CONFIG_ARM_UNALIGNED_ACCESS AND CONFIG_ARM_UNALIGNED_ACCESS)
        if(NEXUS_COMPILER_ARM_GCC OR NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-munaligned-access)
            nexus_log(VERBOSE "  Applied: -munaligned-access")
        endif()
    endif()
endfunction()

##############################################################################
# Advanced Options
##############################################################################

#
# Apply advanced compiler options
#
function(_nexus_apply_advanced_config)
    # Colored diagnostics
    if(DEFINED CONFIG_COMPILER_DIAGNOSTICS_COLOR AND CONFIG_COMPILER_DIAGNOSTICS_COLOR)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_compile_options(-fdiagnostics-color=always)
            nexus_log(VERBOSE "  Applied: -fdiagnostics-color=always")
        elseif(NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-fcolor-diagnostics)
            nexus_log(VERBOSE "  Applied: -fcolor-diagnostics")
        endif()
    endif()

    # Save temporary files
    if(DEFINED CONFIG_COMPILER_SAVE_TEMPS AND CONFIG_COMPILER_SAVE_TEMPS)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-save-temps)
            nexus_log(VERBOSE "  Applied: -save-temps")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/FA)
            nexus_log(VERBOSE "  Applied: /FA (assembly output)")
        elseif(NEXUS_COMPILER_IAR)
            add_compile_options(--preprocess)
            nexus_log(VERBOSE "  Applied: --preprocess")
        endif()
    endif()

    # Compilation time report
    if(DEFINED CONFIG_COMPILER_TIME_REPORT AND CONFIG_COMPILER_TIME_REPORT)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_compile_options(-ftime-report)
            nexus_log(VERBOSE "  Applied: -ftime-report")
        elseif(NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
               NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-ftime-trace)
            nexus_log(VERBOSE "  Applied: -ftime-trace")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/Bt)
            nexus_log(VERBOSE "  Applied: /Bt")
        endif()
    endif()

    # Stack usage information
    if(DEFINED CONFIG_COMPILER_STACK_USAGE AND CONFIG_COMPILER_STACK_USAGE)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_compile_options(-fstack-usage)
            nexus_log(VERBOSE "  Applied: -fstack-usage")
        endif()
    endif()

    # Verbose compiler output
    if(DEFINED CONFIG_COMPILER_VERBOSE AND CONFIG_COMPILER_VERBOSE)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC OR
           NEXUS_COMPILER_CLANG OR NEXUS_COMPILER_APPLECLANG OR
           NEXUS_COMPILER_ARM_CLANG)
            add_compile_options(-v)
            nexus_log(VERBOSE "  Applied: -v (verbose)")
        elseif(NEXUS_COMPILER_MSVC)
            add_compile_options(/verbose)
            nexus_log(VERBOSE "  Applied: /verbose")
        endif()
    endif()
endfunction()

##############################################################################
# Custom Flags
##############################################################################

#
# Apply custom compiler flags
#
function(_nexus_apply_custom_compiler_flags)
    if(DEFINED CONFIG_COMPILER_CUSTOM_FLAGS AND CONFIG_COMPILER_CUSTOM_FLAGS)
        separate_arguments(CUSTOM_FLAGS UNIX_COMMAND "${CONFIG_COMPILER_CUSTOM_FLAGS}")
        add_compile_options(${CUSTOM_FLAGS})
        nexus_log(VERBOSE "  Applied custom flags: ${CONFIG_COMPILER_CUSTOM_FLAGS}")
    endif()
endfunction()

##############################################################################
# Linker Configuration
##############################################################################

#
# Apply memory configuration
#
function(_nexus_apply_memory_config)
    # Stack size
    if(DEFINED CONFIG_STACK_SIZE)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_link_options(-Wl,--defsym,_stack_size=${CONFIG_STACK_SIZE})
            nexus_log(VERBOSE "  Applied: Stack size = ${CONFIG_STACK_SIZE}")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            add_compile_definitions(__STACK_SIZE=${CONFIG_STACK_SIZE})
            nexus_log(VERBOSE "  Applied: __STACK_SIZE = ${CONFIG_STACK_SIZE}")
        elseif(NEXUS_COMPILER_MSVC)
            add_link_options(/STACK:${CONFIG_STACK_SIZE})
            nexus_log(VERBOSE "  Applied: /STACK:${CONFIG_STACK_SIZE}")
        endif()
    endif()

    # Heap size
    if(DEFINED CONFIG_HEAP_SIZE)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_link_options(-Wl,--defsym,_heap_size=${CONFIG_HEAP_SIZE})
            nexus_log(VERBOSE "  Applied: Heap size = ${CONFIG_HEAP_SIZE}")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            add_compile_definitions(__HEAP_SIZE=${CONFIG_HEAP_SIZE})
            nexus_log(VERBOSE "  Applied: __HEAP_SIZE = ${CONFIG_HEAP_SIZE}")
        elseif(NEXUS_COMPILER_MSVC)
            add_link_options(/HEAP:${CONFIG_HEAP_SIZE})
            nexus_log(VERBOSE "  Applied: /HEAP:${CONFIG_HEAP_SIZE}")
        endif()
    endif()
endfunction()

#
# Apply linker optimization
#
function(_nexus_apply_linker_optimization)
    # Garbage collection of unused sections
    if(DEFINED CONFIG_LINKER_GC_SECTIONS AND CONFIG_LINKER_GC_SECTIONS)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_link_options(-Wl,--gc-sections)
            nexus_log(VERBOSE "  Applied: -Wl,--gc-sections")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            add_link_options(--remove)
            nexus_log(VERBOSE "  Applied: --remove")
        elseif(NEXUS_COMPILER_MSVC)
            add_link_options(/OPT:REF)
            nexus_log(VERBOSE "  Applied: /OPT:REF")
        elseif(NEXUS_COMPILER_IAR)
            add_link_options(--vfe)
            nexus_log(VERBOSE "  Applied: --vfe")
        endif()
    endif()

    # Link Time Optimization (already applied in compiler config)
    if(DEFINED CONFIG_LINKER_LTO AND CONFIG_LINKER_LTO)
        nexus_log(VERBOSE "  LTO: Applied via compiler configuration")
    endif()
endfunction()

#
# Apply linker output options
#
function(_nexus_apply_linker_output)
    # Map file generation (handled per-target, not globally)
    if(DEFINED CONFIG_LINKER_MAP_FILE AND CONFIG_LINKER_MAP_FILE)
        nexus_log(VERBOSE "  Map file: Will be generated per-target")
    endif()

    # Cross-reference in map file
    if(DEFINED CONFIG_LINKER_CROSS_REFERENCE AND CONFIG_LINKER_CROSS_REFERENCE)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_link_options(-Wl,--cref)
            nexus_log(VERBOSE "  Applied: -Wl,--cref")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            add_link_options(--xref --callgraph --symbols)
            nexus_log(VERBOSE "  Applied: --xref --callgraph --symbols")
        elseif(NEXUS_COMPILER_IAR)
            add_link_options(--log all)
            nexus_log(VERBOSE "  Applied: --log all")
        endif()
    endif()

    # Print memory usage
    if(DEFINED CONFIG_LINKER_PRINT_MEMORY_USAGE AND CONFIG_LINKER_PRINT_MEMORY_USAGE)
        if(NEXUS_COMPILER_GCC OR NEXUS_COMPILER_ARM_GCC)
            add_link_options(-Wl,--print-memory-usage)
            nexus_log(VERBOSE "  Applied: -Wl,--print-memory-usage")
        elseif(NEXUS_COMPILER_ARM_CLANG)
            add_link_options(--info=summarysizes,sizes,totals --load_addr_map_info)
            nexus_log(VERBOSE "  Applied: --info=summarysizes,sizes,totals --load_addr_map_info")
        endif()
    else()
        # Add basic info output even when not explicitly requested
        if(NEXUS_COMPILER_ARM_CLANG)
            add_link_options(--info=summarysizes)
            nexus_log(VERBOSE "  Applied: --info=summarysizes (default)")
        endif()
    endif()
endfunction()

#
# Apply custom linker flags
#
function(_nexus_apply_custom_linker_flags)
    if(DEFINED CONFIG_LINKER_CUSTOM_FLAGS AND CONFIG_LINKER_CUSTOM_FLAGS)
        separate_arguments(CUSTOM_FLAGS UNIX_COMMAND "${CONFIG_LINKER_CUSTOM_FLAGS}")
        add_link_options(${CUSTOM_FLAGS})
        nexus_log(VERBOSE "  Applied custom linker flags: ${CONFIG_LINKER_CUSTOM_FLAGS}")
    endif()
endfunction()

##############################################################################
# Configuration Summary
##############################################################################

#
# Print configuration summary
#
function(nexus_print_compiler_config_summary)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Compiler Configuration Summary")
    message(STATUS "========================================")

    if(DEFINED CONFIG_COMPILER_UNSIGNED_CHAR AND CONFIG_COMPILER_UNSIGNED_CHAR)
        message(STATUS "  Unsigned char:     Enabled")
    endif()

    if(DEFINED CONFIG_COMPILER_SHORT_ENUMS AND CONFIG_COMPILER_SHORT_ENUMS)
        message(STATUS "  Short enums:       Enabled")
    endif()

    if(DEFINED CONFIG_COMPILER_FUNCTION_SECTIONS AND CONFIG_COMPILER_FUNCTION_SECTIONS)
        message(STATUS "  Function sections: Enabled")
    endif()

    if(DEFINED CONFIG_COMPILER_WARNING_LEVEL)
        message(STATUS "  Warning level:     ${CONFIG_COMPILER_WARNING_LEVEL}")
    endif()

    if(DEFINED CONFIG_COMPILER_DEBUG_FORMAT)
        message(STATUS "  Debug format:      ${CONFIG_COMPILER_DEBUG_FORMAT}")
    endif()

    if(DEFINED CONFIG_COMPILER_LTO AND CONFIG_COMPILER_LTO)
        message(STATUS "  LTO:               Enabled")
    endif()

    if(DEFINED CONFIG_COMPILER_USE_MICROLIB AND CONFIG_COMPILER_USE_MICROLIB)
        message(STATUS "  Standard library:  Microlib")
    elseif(DEFINED CONFIG_COMPILER_USE_NANO AND CONFIG_COMPILER_USE_NANO)
        message(STATUS "  Standard library:  Newlib-nano")
    endif()

    if(DEFINED CONFIG_LINKER_GC_SECTIONS AND CONFIG_LINKER_GC_SECTIONS)
        message(STATUS "  Linker GC:         Enabled")
    endif()

    message(STATUS "========================================")
    message(STATUS "")
endfunction()

message(STATUS "NexusCompilerConfig module loaded")

##############################################################################
# End of NexusCompilerConfig.cmake
##############################################################################

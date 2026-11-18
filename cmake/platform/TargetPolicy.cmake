# Target properties carry policy across consumer directory boundaries.
include_guard(GLOBAL)

function(nexus_require_build_generator)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja" OR CMAKE_CONFIGURATION_TYPES)
        message(
            FATAL_ERROR
            "Nexus supports single-configuration Ninja; select -G Ninja"
        )
    endif()
endfunction()

function(nexus_apply_target_policy target)
    if(NOT TARGET "${target}" OR NOT TARGET Nexus::Config)
        message(FATAL_ERROR "Target policy requires a target and Nexus::Config")
    endif()
    get_target_property(lto Nexus::Config NEXUS_LTO_REQUIRED)
    if(lto)
        # CMAKE_INTERPROCEDURAL_OPTIMIZATION initializes targets only in the
        # defining directory. Public helpers may be called by a parent project.
        set_property(
            TARGET "${target}"
            PROPERTY INTERPROCEDURAL_OPTIMIZATION ON
        )
    else()
        set_property(
            TARGET "${target}"
            PROPERTY INTERPROCEDURAL_OPTIMIZATION OFF
        )
    endif()
    get_target_property(launcher Nexus::Config NEXUS_COMPILER_LAUNCHER)
    if("${launcher}" MATCHES "-NOTFOUND$")
        set(launcher "")
    endif()
    # Helpers can create targets in a consuming parent directory. A child
    # CMAKE_C_STANDARD does not reach that scope, so each owned C target must
    # carry the exact language contract. C++ test standards remain independent.
    set_target_properties(
        "${target}"
        PROPERTIES
            C_STANDARD 11
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
            C_COMPILER_LAUNCHER "${launcher}"
            CXX_COMPILER_LAUNCHER "${launcher}"
    )
endfunction()

function(nexus_apply_firmware_policy target)
    if(NOT TARGET "${target}" OR NOT TARGET nexus_firmware_options)
        message(FATAL_ERROR "Firmware policy requires a configured target")
    endif()
    get_target_property(kind "${target}" TYPE)
    get_target_property(imported "${target}" IMPORTED)
    if(imported OR NOT kind STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "Firmware policy requires a local executable")
    endif()
    nexus_apply_target_policy("${target}")
    # Runtime specs and reset entry policy belong to the final image. Ordinary
    # libraries expose ABI requirements without selecting a firmware runtime.
    target_link_libraries(
        "${target}"
        PRIVATE nexus_build_options nexus_firmware_options
    )
endfunction()

function(nexus_target_warnings target)
    cmake_parse_arguments(WARN "STRICT" "" "" ${ARGN})
    if(WARN_UNPARSED_ARGUMENTS OR NOT WARN_STRICT)
        message(FATAL_ERROR "nexus_target_warnings requires target STRICT")
    endif()
    if(NOT TARGET "${target}" OR NOT TARGET nexus_owned_warnings)
        message(FATAL_ERROR "Strict warnings require a configured Nexus target")
    endif()
    # Platform targets are strict internally. Products choose their diagnostics
    # explicitly; ABI and selected optimization remain mandatory.
    target_link_libraries("${target}" PRIVATE nexus_owned_warnings)
endfunction()

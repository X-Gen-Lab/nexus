# Standalone C11 integration programs retain assertions in every Native mode.
# Author: Nexus Team
include_guard(GLOBAL)

function(nexus_c_integration_test name)
    cmake_parse_arguments(
        ARG
        ""
        ""
        "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS"
        ${ARGN}
    )
    add_executable(${name} ${ARG_SOURCES})
    target_compile_features(${name} PRIVATE c_std_11)
    set_target_properties(${name} PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(
        ${name}
        PRIVATE nexus_build_options nexus_owned_warnings ${ARG_LIBRARIES}
    )
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    target_compile_definitions(${name} PRIVATE ${ARG_DEFINITIONS})
    if(MSVC)
        target_compile_options(${name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(
        ${name}
        PROPERTIES TIMEOUT 30 LABELS "host;integration;c-api"
    )
endfunction()

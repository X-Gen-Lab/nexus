# Shared host behavior registration; firmware remains C11.
# Author: Nexus Team
# Preserve complete host DWARF without repeating uncompressed framework debug
# sections in every contract executable. Firmware never enters this directory.
set(nexus_google_compressed_debug_enabled FALSE)
if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckLinkerFlag)
    check_linker_flag(
        CXX
        "-Wl,--compress-debug-sections=zlib-gabi"
        nexus_google_compressed_debug
    )
    if(nexus_google_compressed_debug)
        set(nexus_google_compressed_debug_enabled TRUE)
    endif()
endif()

function(nexus_google_test name)
    cmake_parse_arguments(
        ARG
        "STM32F4_LL"
        ""
        "SOURCES;LIBRARIES;INCLUDES"
        ${ARGN}
    )
    add_executable(${name} ${ARG_SOURCES})
    # Only generated bindings use the compile-visible Core coordinator. Keep
    # that private include off unrelated test sources and compiler contexts.
    foreach(source IN LISTS ARG_SOURCES)
        if(source MATCHES "/bindings\\.c$")
            set_property(
                SOURCE "${source}"
                APPEND
                PROPERTY INCLUDE_DIRECTORIES "${NEXUS_SOURCE_DIR}/core/private"
            )
        endif()
    endforeach()
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(
        ${name}
        PRIVATE
            nexus_build_options
            nexus_owned_warnings
            GTest::gmock_main
            ${ARG_LIBRARIES}
    )
    nexus_use_cmsis_host_model(${name})
    if(nexus_google_compressed_debug_enabled)
        target_link_options(
            ${name}
            PRIVATE "-Wl,--compress-debug-sections=zlib-gabi"
        )
    endif()
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    if(ARG_STM32F4_LL)
        # Match the concrete SoC target's private I/O implementation dependency.
        # Provider headers may expose private sequence state to their C models;
        # this directory never becomes a public consumer include.
        target_include_directories(
            ${name}
            PRIVATE "${NEXUS_SOURCE_DIR}/io/private"
        )
        include("${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sdk/STM32Official.cmake")
        nexus_stm32f4_official(${name})
    endif()
    set_property(TARGET ${name} PROPERTY NEXUS_GOOGLE_CONTRACT TRUE)
    add_dependencies(nexus_google_contracts ${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(
        ${name}
        PROPERTIES
            TIMEOUT 30
            LABELS "host;behavior;GoogleTest;${nexus_behavior_domain}"
    )
endfunction()

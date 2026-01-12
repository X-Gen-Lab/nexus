##############################################################################
# Nexus CMake Helper Functions
##############################################################################

# Add a Nexus library with standard settings
function(nexus_add_library TARGET)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES;DEPS" ${ARGN})

    add_library(${TARGET} STATIC ${ARG_SOURCES})

    target_include_directories(${TARGET}
        PUBLIC
            ${ARG_INCLUDES}
    )

    if(ARG_DEPS)
        target_link_libraries(${TARGET} PUBLIC ${ARG_DEPS})
    endif()

    # Apply standard compile options
    target_compile_options(${TARGET} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endfunction()

# Add a Nexus executable with standard settings
function(nexus_add_executable TARGET)
    cmake_parse_arguments(ARG "" "LINKER_SCRIPT" "SOURCES;DEPS" ${ARGN})

    add_executable(${TARGET} ${ARG_SOURCES})

    if(ARG_DEPS)
        target_link_libraries(${TARGET} PRIVATE ${ARG_DEPS})
    endif()

    if(ARG_LINKER_SCRIPT)
        target_link_options(${TARGET} PRIVATE
            -T${ARG_LINKER_SCRIPT}
            -Wl,-Map=${TARGET}.map
        )
        set_target_properties(${TARGET} PROPERTIES
            LINK_DEPENDS ${ARG_LINKER_SCRIPT}
        )
    endif()

    # Generate binary files for embedded targets
    if(NOT NEXUS_PLATFORM STREQUAL "native")
        nexus_generate_binary(${TARGET})
    endif()
endfunction()

# Add a Nexus test executable
function(nexus_add_test TARGET)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPS" ${ARGN})

    add_executable(${TARGET} ${ARG_SOURCES})

    target_link_libraries(${TARGET}
        PRIVATE
            ${ARG_DEPS}
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
    )

    # Register with CTest
    gtest_discover_tests(${TARGET})
endfunction()

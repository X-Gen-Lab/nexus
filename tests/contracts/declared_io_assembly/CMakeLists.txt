cmake_minimum_required(VERSION 3.31)
project(declared_io_link_fixture LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Fixture needs explicit source root and one assembly")
endif()
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM fixture")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "External explicit fixture main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(declared_io_fixture SOURCES main.c)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
include("${fixture_config}/selection.cmake")
foreach(kind gpio exti uart adc can)
    if(kind IN_LIST NEXUS_SELECTED_KINDS)
        string(TOUPPER "${kind}" kind_upper)
        target_compile_definitions(declared_io_fixture PRIVATE
            "NX_DECLARED_${kind_upper}")
    endif()
endforeach()
# Optional word faces have their own factory; controller kind remains UART.
# Read the generated public declaration rather than infer it from a filename.
file(READ "${fixture_config}/nexus_factory.h" fixture_factory)
string(FIND "${fixture_factory}" "nx_factory_uart_word(" fixture_word_face)
if(NOT fixture_word_face EQUAL -1)
    target_compile_definitions(declared_io_fixture PRIVATE NX_DECLARED_UART_WORD)
endif()
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
# Linked methods and their actual provider bytes are checked after section GC.
set_property(TARGET declared_io_fixture APPEND PROPERTY LINK_DEPENDS
    "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py")
add_custom_command(TARGET declared_io_fixture POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:declared_io_fixture>"
        --bindings "${fixture_config}/bindings.c"
        --resolved "${fixture_config}/resolved.json"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)

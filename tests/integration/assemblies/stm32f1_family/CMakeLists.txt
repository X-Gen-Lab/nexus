cmake_minimum_required(VERSION 3.31)
project(nexus_f1_family_reference LANGUAGES C ASM)

if(NOT NEXUS_SOURCE_ROOT)
    enable_language(CXX)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Select an exact STM32F1 reference assembly")
endif()

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(nexus_f1_family_consumer SOURCES main.c)
nexus_target_warnings(nexus_f1_family_consumer STRICT)

if(NOT NEXUS_SOURCE_ROOT)
    # Check the public C ABI without adding C++ to the firmware image.
    add_library(nexus_f1_public_cpp OBJECT public_cpp.cpp)
    target_link_libraries(nexus_f1_public_cpp PRIVATE Nexus::Platform)
    nexus_target_warnings(nexus_f1_public_cpp STRICT)
    add_dependencies(nexus_f1_family_consumer nexus_f1_public_cpp)
endif()

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(nexus_source Nexus::Config NEXUS_SOURCE_DIR)
get_target_property(nexus_bundle Nexus::Platform NEXUS_CONFIG_DIR)
set(retention_report "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json")
add_custom_command(
    TARGET nexus_f1_family_consumer
    POST_BUILD
    BYPRODUCTS "${retention_report}"
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${nexus_source}/tools/measurement/retention.py" --elf
        "$<TARGET_FILE:nexus_f1_family_consumer>" --bindings
        "${nexus_bundle}/bindings.c" --resolved "${nexus_bundle}/resolved.json"
        --output "${retention_report}"
    VERBATIM
)

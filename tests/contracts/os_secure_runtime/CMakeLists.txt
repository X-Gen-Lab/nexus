# Separate images share the sole production Runtime CPU/ABI source graph.
cmake_minimum_required(VERSION 3.31)
project(nexus_secure_runtime_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_RUNTIME_ASSEMBLY)
    message(FATAL_ERROR "An explicit software-reference assembly is required")
endif()
if(NEXUS_SOURCE_SDK_PREFIX)
    find_package(Nexus REQUIRED COMPONENTS Runtime)
else()
    get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
    include("${nexus_source}/cmake/platform/Runtime.cmake")
endif()
nexus_add_runtime(ASSEMBLY "${NEXUS_RUNTIME_ASSEMBLY}")
if(NEXUS_SECURE_STAGE STREQUAL "secure")
    nexus_enable_secure_context()
    add_executable(nexus_secure_contract secure.c)
    target_link_libraries(nexus_secure_contract PRIVATE Nexus::SecureContext
        Nexus::Runtime)
    target_link_options(nexus_secure_contract PRIVATE
        "-T${CMAKE_CURRENT_LIST_DIR}/secure.ld"
        "-Wl,--cmse-implib,--out-implib=${CMAKE_CURRENT_BINARY_DIR}/secure-import.o"
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/nexus_secure_contract.map")
    set_target_properties(nexus_secure_contract PROPERTIES SUFFIX ".elf")
elseif(NEXUS_SECURE_STAGE STREQUAL "nonsecure")
    if(NOT EXISTS "${NEXUS_SECURE_IMPORT_LIBRARY}")
        message(FATAL_ERROR "The actual Secure gateway import library is required")
    endif()
    add_executable(nexus_nonsecure_contract nonsecure.c)
    target_link_libraries(nexus_nonsecure_contract PRIVATE Nexus::Runtime
        "${NEXUS_SECURE_IMPORT_LIBRARY}")
    target_link_options(nexus_nonsecure_contract PRIVATE
        "-T${CMAKE_CURRENT_LIST_DIR}/nonsecure.ld"
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/nexus_nonsecure_contract.map")
    set_target_properties(nexus_nonsecure_contract PROPERTIES SUFFIX ".elf")
else()
    message(FATAL_ERROR "NEXUS_SECURE_STAGE must be secure or nonsecure")
endif()

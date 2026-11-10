cmake_minimum_required(VERSION 3.31)
project(nexus_f407_i2s2_public LANGUAGES C ASM)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(FATAL_ERROR "Explicit Nexus source and authored assembly are required")
endif()
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus)
nexus_add_firmware(f407_i2s2_public
    SOURCES "${NEXUS_SOURCE_ROOT}/cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c"
    LIBRARIES Nexus::IO)
set_target_properties(f407_i2s2_public PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
nexus_target_warnings(f407_i2s2_public STRICT)

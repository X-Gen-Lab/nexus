cmake_minimum_required(VERSION 3.31)
project(nexus_f103_standby_reference LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/f103rb-pa0.toml"
    CACHE FILEPATH
    "Exact F103RB PA0 Standby assembly"
)
if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(nexus_f103_standby_consumer SOURCES main.c)
nexus_target_warnings(nexus_f103_standby_consumer STRICT)

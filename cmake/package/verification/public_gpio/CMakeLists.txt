cmake_minimum_required(VERSION 3.31)
project(nexus_public_gpio LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml"
    CACHE FILEPATH
    "This consumer owns its assembly"
)

find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(nexus_public_gpio SOURCES main.c)

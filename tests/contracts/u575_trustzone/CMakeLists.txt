cmake_minimum_required(VERSION 3.31)
project(nexus_u575_trustzone_reference LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
get_filename_component(nexus_source "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${nexus_source}/cmake/platform/Runtime.cmake")
if(NEXUS_TRUSTZONE_STAGE STREQUAL "secure")
    nexus_add_runtime(ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/secure.toml")
    add_executable(u575_secure secure.c secure_start.S
        "${nexus_source}/soc/stm32u575/security.c"
        "${nexus_source}/soc/stm32u575/drivers/gpio.c")
    target_link_libraries(u575_secure PRIVATE Nexus::Runtime
        Nexus::UserIoBroker nexus_owned_warnings)
    target_include_directories(u575_secure PRIVATE
        "${nexus_source}/soc/stm32u575/private"
        "${nexus_source}/components/user_io_broker/private")
    target_include_directories(u575_secure SYSTEM PRIVATE
        "${nexus_source}/vendors/st/cmsis_device_u5/Include"
        "${nexus_source}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(u575_secure PRIVATE STM32U575xx)
    target_link_options(u575_secure PRIVATE
        "-T${CMAKE_CURRENT_LIST_DIR}/secure.ld"
        "-Wl,--cmse-implib,--out-implib=${CMAKE_CURRENT_BINARY_DIR}/secure-import.o"
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/u575_secure.map"
        "-Wl,--build-id=none")
    set_target_properties(u575_secure PROPERTIES SUFFIX ".elf")
elseif(NEXUS_TRUSTZONE_STAGE STREQUAL "nonsecure")
    if(NOT EXISTS "${NEXUS_SECURE_IMPORT_LIBRARY}")
        message(FATAL_ERROR "Actual Secure SG import library required")
    endif()
    nexus_add_runtime(ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/nonsecure.toml")
    add_executable(u575_nonsecure nonsecure.c nonsecure_start.S)
    target_link_libraries(u575_nonsecure PRIVATE Nexus::Runtime
        nexus_owned_warnings "${NEXUS_SECURE_IMPORT_LIBRARY}")
    target_include_directories(u575_nonsecure PRIVATE
        "${nexus_source}/components/user_io_broker/include")
    target_link_options(u575_nonsecure PRIVATE
        "-T${CMAKE_CURRENT_LIST_DIR}/nonsecure.ld"
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/u575_nonsecure.map"
        "-Wl,--build-id=none")
    set_target_properties(u575_nonsecure PROPERTIES SUFFIX ".elf")
else()
    message(FATAL_ERROR "Explicit secure/nonsecure reference stage required")
endif()

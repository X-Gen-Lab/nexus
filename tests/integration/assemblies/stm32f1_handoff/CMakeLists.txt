cmake_minimum_required(VERSION 3.31)
project(nexus_f103rb_handoff_reference LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Nexus CONFIG REQUIRED)

if(NEXUS_HANDOFF_ROLE STREQUAL "app")
    nexus_add_platform(
        ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/../stm32f1_family/f103rb-offset-baremetal.toml"
    )
    nexus_add_firmware(nexus_f103rb_handoff_app SOURCES app.c)
    nexus_target_warnings(nexus_f103rb_handoff_app STRICT)
elseif(NEXUS_HANDOFF_ROLE STREQUAL "boot")
    if(NOT EXISTS "${NEXUS_HANDOFF_CONTRACT_DIR}/target_contract.h")
        message(
            FATAL_ERROR
            "Link and verify the exact offset app before building boot"
        )
    endif()
    nexus_add_platform(ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/boot.toml")
    nexus_add_firmware(nexus_f103rb_handoff_boot SOURCES boot.c)
    nexus_target_warnings(nexus_f103rb_handoff_boot STRICT)
    target_include_directories(
        nexus_f103rb_handoff_boot
        PRIVATE "${NEXUS_HANDOFF_CONTRACT_DIR}"
    )
    target_link_libraries(
        nexus_f103rb_handoff_boot
        PRIVATE Nexus::STM32F103RBHandoff
    )
else()
    message(
        FATAL_ERROR
        "The handoff fixture requires NEXUS_HANDOFF_ROLE=app or boot"
    )
endif()

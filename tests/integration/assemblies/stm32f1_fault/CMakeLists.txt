cmake_minimum_required(VERSION 3.31)
project(nexus_stm32f1_fault_reference LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(
    ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/../stm32f1_family/f103rb-baremetal.toml"
)
nexus_add_firmware(
    nexus_f103rb_fault_selected
    SOURCES main.c
    LIBRARIES Nexus::STM32F103RBFault
)
nexus_target_warnings(nexus_f103rb_fault_selected STRICT)
target_compile_definitions(
    nexus_f103rb_fault_selected
    PRIVATE NX_F103RB_FAULT_REFERENCE=1
)

nexus_add_firmware(nexus_f103rb_fault_unselected SOURCES main.c)
nexus_target_warnings(nexus_f103rb_fault_unselected STRICT)

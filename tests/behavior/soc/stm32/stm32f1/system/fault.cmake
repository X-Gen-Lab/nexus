# Exercise exact F103RB capture using the real portable retained recorder.
# Author: Nexus Team
set(f1_fault_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103rb_fault_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/fault_test.cpp"
        "${f1_fault_package}/system/fault.c"
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}" "${f1_fault_package}/include"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::Fault
)
nexus_use_cmsis_core(stm32f103rb_fault_contracts)
target_include_directories(
    stm32f103rb_fault_contracts
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(stm32f103rb_fault_contracts PRIVATE STM32F103xB)
target_compile_options(
    stm32f103rb_fault_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/fault_model.h"
)

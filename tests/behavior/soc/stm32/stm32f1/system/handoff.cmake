# Verify the exact F103RB masked entry against the real ClockDomain component.
# Author: Nexus Team
set(f1_handoff_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    stm32f103rb_handoff_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/handoff_test.cpp"
        "${f1_handoff_package}/system/handoff.c"
    INCLUDES
        "${CMAKE_CURRENT_LIST_DIR}"
        "${f1_handoff_package}"
        "${f1_handoff_package}/include"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::ClockDomain
)
nexus_use_cmsis_core(stm32f103rb_handoff_contracts)
target_include_directories(
    stm32f103rb_handoff_contracts
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(
    stm32f103rb_handoff_contracts
    PRIVATE STM32F103xB NEXUS_F103_HANDOFF_MODEL
)

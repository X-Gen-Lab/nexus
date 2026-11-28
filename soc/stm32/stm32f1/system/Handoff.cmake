# Explicit opt-in Cortex-M3 transfer for the reviewed F103RB bare-metal layout.
# Author: Nexus Team
if(
    NEXUS_EXACT_PART STREQUAL "STM32F103RBT6"
    AND NEXUS_BACKEND STREQUAL "baremetal"
)
    add_library(
        nexus_stm32f103rb_handoff
        STATIC
        EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_LIST_DIR}/handoff.c"
        "${CMAKE_CURRENT_LIST_DIR}/handoff_tail.S"
    )
    nexus_apply_target_policy(nexus_stm32f103rb_handoff)
    target_include_directories(
        nexus_stm32f103rb_handoff
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include"
    )
    target_link_libraries(
        nexus_stm32f103rb_handoff
        PUBLIC Nexus::ClockDomain
        PRIVATE
            nexus_stm32f1_startup_usage
            Nexus::Arch
            nexus_build_options
            nexus_owned_warnings
    )
    add_library(Nexus::STM32F103RBHandoff ALIAS nexus_stm32f103rb_handoff)
endif()

# Compile the retained backup owner only for the reviewed read-only mode.
# Author: Nexus Team
nexus_controller_modes(f1_rtc rtc)
if("backup-adopt" IN_LIST f1_rtc OR "backup-provision" IN_LIST f1_rtc)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/counter.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IORTC)
endif()
if("backup-provision" IN_LIST f1_rtc)
    if(
        NOT NEXUS_EXACT_PART STREQUAL "STM32F103RBT6"
        OR NOT NEXUS_BACKEND STREQUAL "baremetal"
    )
        message(FATAL_ERROR "RTC backup provisioning requires F103RB baremetal")
    endif()
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_RTC_PROVISION=1
    )
endif()

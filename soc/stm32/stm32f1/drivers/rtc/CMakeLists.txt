# Compile the retained backup owner only for the reviewed read-only mode.
# Author: Nexus Team
nexus_controller_modes(f1_rtc rtc)
if("backup-adopt" IN_LIST f1_rtc)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/counter.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IORTC)
endif()

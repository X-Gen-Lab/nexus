# Selected can sources and private dependencies.
nexus_controller_modes(can_modes can)
if(can_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/can.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/common/bxcan/can_controller.c"
    )
endif()

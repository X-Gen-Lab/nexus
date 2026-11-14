# Selected can sources and private dependencies.
nexus_controller_modes(can_modes can)
if(can_modes)
    set(stm32_gpio_helpers ON)
    target_sources(nexus_soc_stm32f4 PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/can.c"
        "${CMAKE_CURRENT_LIST_DIR}/../ip/classic_can/can_controller.c")
endif()

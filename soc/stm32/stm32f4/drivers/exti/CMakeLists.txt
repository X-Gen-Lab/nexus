# Selected exti sources and private dependencies.
nexus_controller_modes(exti_modes exti)
if("hardware-event" IN_LIST exti_modes)
    set(stm32_gpio_helpers ON)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/exti_hardware.c"
    )
endif()
if("edge-event" IN_LIST exti_modes OR "pvd-events" IN_LIST exti_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/exti.c")
endif()
if("edge-event" IN_LIST exti_modes)
    set(stm32_gpio_helpers ON)
endif()
if("pvd-level" IN_LIST exti_modes OR "pvd-events" IN_LIST exti_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pvd.c")
endif()

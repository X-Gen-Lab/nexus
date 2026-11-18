# Selected gpio sources and private dependencies.
nexus_controller_modes(gpio_modes gpio)
if(gpio_modes)
    set(stm32_gpio_helpers ON)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/lifecycle_gpio.c"
    )
endif()

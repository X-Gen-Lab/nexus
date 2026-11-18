# Selected capture sources and private dependencies.
nexus_controller_modes(capture_modes capture)
if(capture_modes)
    set(stm32_gpio_helpers ON)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/capture.c"
            "${CMAKE_CURRENT_LIST_DIR}/../ip/av/av.c"
    )
    nexus_stm32f407_capture_hal(nexus_soc_stm32f4)
    target_compile_definitions(
        nexus_soc_stm32f4
        PRIVATE NX_STM32_CAPTURE_MAX_BITS=NEXUS_STM32_CAPTURE_MAX_BITS
    )
endif()

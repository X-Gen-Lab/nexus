# Selected i2c sources and private dependencies.
nexus_controller_modes(i2c_modes i2c)
if(i2c_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/i2c.c"
            "${CMAKE_CURRENT_LIST_DIR}/i2c_cold.c"
    )
    if("irq-messages" IN_LIST i2c_modes OR "dma-messages" IN_LIST i2c_modes)
        target_sources(
            nexus_soc_stm32f4
            PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}/i2c_async.c"
                "${CMAKE_CURRENT_LIST_DIR}/../ip/i2c_v1_async/i2c_v1_async.c"
        )
        target_compile_definitions(
            nexus_soc_stm32f4
            PRIVATE "NX_I2C_V1_PORT_HEADER=\"drivers/i2c/stm32f407_i2c_port.h\""
        )
    endif()
endif()
nexus_controller_modes(i2c_target_modes i2c_target)
if(i2c_target_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/i2c_target.c"
    )
endif()

# Select reviewed F103 I2C1 personalities and their common cold receipt.
# Author: Nexus Team
nexus_controller_modes(i2c_modes i2c)
if("poll-messages" IN_LIST i2c_modes OR f1_i2c_target)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/i2c.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOI2C)
endif()
if(f1_i2c_target)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/target.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOI2CTarget)
endif()
if("target-tx-irq" IN_LIST f1_i2c_target)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_I2C_TARGET_TX=1
    )
elseif("target-write-read-irq" IN_LIST f1_i2c_target)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_I2C_TARGET_WRITE_READ=1
    )
endif()

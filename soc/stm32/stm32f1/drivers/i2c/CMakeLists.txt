# Select reviewed F103 I2C1 personalities and their common cold receipt.
# Author: Nexus Team
nexus_controller_modes(i2c_modes i2c)
if(i2c_modes OR f1_i2c_target)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/i2c.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOI2C)
endif()
if("irq-write-messages" IN_LIST i2c_modes)
    if(NOT NEXUS_EXACT_PART STREQUAL "STM32F103RBT6")
        message(FATAL_ERROR "I2C IRQ writer requires exact F103RB")
    endif()
    set(f1_i2c_v1 "${CMAKE_CURRENT_LIST_DIR}/../../../common/i2c_v1_async")
    target_sources(
        nexus_soc_stm32f1
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/master_irq.c"
            "${f1_i2c_v1}/i2c_v1_async.c"
    )
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_I2C_V1_F1_WRITE_ONLY=1
    )
    target_include_directories(nexus_soc_stm32f1 PRIVATE "${f1_i2c_v1}")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODMA)
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

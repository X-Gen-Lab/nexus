# Exercise unchanged official LL functions through external MMIO/CPU ports.
nexus_google_test(nexus_crc_test
    SOURCES crc_test.cpp LIBRARIES Nexus::IOCRC)
nexus_google_test(nexus_native_chip_test
    SOURCES native_chip_test.cpp
        "${NEXUS_SOURCE_DIR}/tests/support/native/drivers/chip.c"
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_stm32_crc_test STM32F4_LL
    SOURCES stm32_crc_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/crc.c"
    LIBRARIES Nexus::IOCRC Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_crc_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_crc_test)
target_compile_definitions(nexus_stm32_crc_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_crc_model_privileged
    nx_arch_in_isr=nx_crc_model_isr
    nx_arch_irq_is_masked=nx_crc_model_masked
    nx_arch_irq_save=nx_crc_model_save
    nx_arch_irq_restore=nx_crc_model_restore
    nx_arch_dsb=nx_crc_model_dsb)
target_compile_options(nexus_stm32_crc_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_crc_model.h")

nexus_google_test(nexus_stm32_chip_test STM32F4_LL
    SOURCES stm32_chip_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/chip.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_chip_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_chip_test)
target_compile_definitions(nexus_stm32_chip_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_chip_model_privileged)
target_compile_options(nexus_stm32_chip_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_chip_model.h")

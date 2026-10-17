# Common contracts and real G474 MMIO providers remain separate test targets.
nexus_google_test(nexus_can_fd_contract_test
    SOURCES can_fd_contract_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_g474_fdcan_test
    SOURCES g474_fdcan_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/fdcan.c"
    LIBRARIES Nexus::IO Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/io/private")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_g474_fdcan_test g4)
target_include_directories(nexus_g474_fdcan_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_fdcan_test PRIVATE STM32G474xx
    USE_RTOS=0 USE_HAL_FDCAN_REGISTER_CALLBACKS=0)
target_compile_options(nexus_g474_fdcan_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_fdcan_model.h")

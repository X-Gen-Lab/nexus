# Production C backend and portable owner; HAL calls are the mocked boundary.
nexus_google_test(nexus_h743_w25q128_test SOURCES h743_w25q128_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/w25q128_mapped.c"
    LIBRARIES Nexus::MappedStorage Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h743_w25q128/private")
target_include_directories(nexus_h743_w25q128_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc")
nexus_use_cmsis_core(nexus_h743_w25q128_test)
target_compile_definitions(nexus_h743_w25q128_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_w25q128_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_w25q128_model.h")

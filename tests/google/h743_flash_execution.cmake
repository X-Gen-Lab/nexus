# Exact physical image addresses and real H743 controller drain implementation.
# Root includes this independent fragment during its serialized configure window.
nexus_google_test(nexus_h743_flash_execution_test
    SOURCES h743_flash_execution_test.cpp h743_model.cpp
        h743_flash_image_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash_execution.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_flash_execution_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_flash_execution_test)
target_compile_definitions(nexus_h743_flash_execution_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_flash_execution_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_execution_model.h"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_image_model.h")

target_sources(nexus_h743_flash_execution_test PRIVATE h743_flash_fixture.cpp
    h743_flash_ram_closure_model.cpp h743_flash_mpu_model.cpp
    ${h743_flash_ram_production_sources})
target_compile_options(nexus_h743_flash_execution_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_mpu_model.h")

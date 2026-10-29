# D2 test-only fragment. Root registers after the first RAM leaf is real.
nexus_google_test(nexus_h743_flash_ram_closure_test
    SOURCES h743_flash_ram_closure_test.cpp h743_flash_ram_closure_model.cpp
        h743_flash_mpu_model.cpp h743_flash_ram_observer.cpp
        h743_model.cpp h743_flash_image_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/h743_flash_geometry.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash_execution.c"
        ${h743_flash_ram_production_sources}
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_flash_ram_closure_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_flash_ram_closure_test)
target_compile_definitions(nexus_h743_flash_ram_closure_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_flash_ram_closure_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_mpu_model.h"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_image_model.h")
# Root adds only the real RAM leaf/owner C sources after intended symbol RED.

# Reuse the existing exact high TOML bundle and real high RCC/System dispatchers.
# This fragment must follow the maintained h743_high.cmake include.
nexus_google_test(nexus_h743_flash_ram_generated_test
    SOURCES h743_flash_ram_generated_test.cpp h743_flash_ram_closure_model.cpp
        h743_flash_mpu_model.cpp h743_flash_ram_observer.cpp
        "${h7_high_bundle}/bindings.c" ${h743_high_model_sources}
        h743_flash_image_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi_wire.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/pwm.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash_execution.c"
        ${h743_flash_ram_production_sources}
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2/i2c_v2.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_high_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2")
target_include_directories(nexus_h743_flash_ram_generated_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_flash_ram_generated_test)
target_compile_definitions(nexus_h743_flash_ram_generated_test PRIVATE
    STM32H743xx _init=nx_h743_ram_fixture_init _fini=nx_h743_ram_fixture_fini)
target_compile_options(nexus_h743_flash_ram_generated_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_ram_high_model.h")
target_link_options(nexus_h743_flash_ram_generated_test PRIVATE
    "-Wl,--wrap=nx_stm32h743_flash_ram_initialize")

# Exact selected-object constructor boundary; hardware ports remain external.
nexus_google_test(nexus_h743_flash_fixed_storage_test
    SOURCES h743_flash_fixed_storage_test.cpp h743_flash_ram_closure_model.cpp
        h743_flash_mpu_model.cpp h743_flash_ram_observer.cpp
        h743_model.cpp h743_flash_image_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/h743_flash_geometry.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash_execution.c"
        ${h743_flash_ram_production_sources}
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_flash_fixed_storage_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_flash_fixed_storage_test)
target_compile_definitions(nexus_h743_flash_fixed_storage_test PRIVATE
    STM32H743xx NX_H743_FLASH_RAM_FIXED_STORAGE=1)
target_compile_options(nexus_h743_flash_fixed_storage_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_mpu_model.h"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_image_model.h")

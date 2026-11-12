# Execute the concrete Platform resource owner; model only external CPU/MMIO.
nexus_google_test(nexus_stm32_platform_cold_test STM32F4_LL
    SOURCES stm32_platform_cold_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_platform_cold_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_platform_cold_test PRIVATE STM32F407xx NX_I2C_MULTI_STM32)
target_compile_options(nexus_stm32_platform_cold_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_platform_cold_model.h")

# Pin capture/restore are independently tested production GPIO methods. These
# aliases expose only their external port so readback/refusal remains observable.
add_library(nexus_stm32_platform_cold_gpio OBJECT
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c")
target_link_libraries(nexus_stm32_platform_cold_gpio PRIVATE
    Nexus::IO nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_stm32_platform_cold_gpio PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
    "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
    "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_platform_cold_gpio SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_platform_cold_gpio PRIVATE
    STM32F407xx NX_I2C_MULTI_STM32 nx_stm32_pin_capture=nx_platform_actual_pin_capture
    nx_stm32_pin_restore=nx_platform_actual_pin_restore)
target_compile_options(nexus_stm32_platform_cold_gpio PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_platform_cold_model.h")
nexus_stm32f4_official(nexus_stm32_platform_cold_gpio)
target_sources(nexus_stm32_platform_cold_test PRIVATE
    $<TARGET_OBJECTS:nexus_stm32_platform_cold_gpio>)

set(family_irq_config "${CMAKE_CURRENT_BINARY_DIR}/family-irq-config")
file(MAKE_DIRECTORY "${family_irq_config}")
file(CONFIGURE OUTPUT "${family_irq_config}/nexus_config.h" CONTENT [=[
#define NEXUS_IRQ_KERNEL_POLICY 2U
#define NEXUS_IRQ_PRIORITY_BITS 4U
#define NEXUS_IRQ_SYSCALL_PRIORITY 5U
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 97U
]=] @ONLY)
nexus_google_test(nexus_stm32_f446_irq_banks_test STM32F4_LL
    SOURCES stm32_family_irq_banks_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
    LIBRARIES Nexus::IO
    INCLUDES "${family_irq_config}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_f446_irq_banks_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_f446_irq_banks_test PRIVATE
    STM32F446xx NX_I2C_MULTI_STM32)
target_compile_options(nexus_stm32_f446_irq_banks_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_family_irq_banks_model.h")

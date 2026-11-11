# Retain the real supplier LL bodies and only model external MMIO/CPU events.
nexus_google_test(nexus_dac_tx_test
    SOURCES dac_tx_test.cpp LIBRARIES Nexus::IODACTx)
set(dac_dma_model_config "${CMAKE_CURRENT_BINARY_DIR}/dac-dma-model-config")
file(MAKE_DIRECTORY "${dac_dma_model_config}")
file(CONFIGURE OUTPUT "${dac_dma_model_config}/nexus_config.h"
    CONTENT "#define NEXUS_IRQ_KERNEL_POLICY 2\n#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 82\n#define NEXUS_IRQ_PRIORITY_BITS 4\n#define NEXUS_IRQ_SYSCALL_PRIORITY 5\n" @ONLY)
nexus_google_test(nexus_stm32_dac_dma_test STM32F4_LL
    SOURCES stm32_dac_dma_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/dac_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/dac.c"
    LIBRARIES Nexus::IODACTx Nexus::IODAC Nexus::IODMA Nexus::IOWake Nexus::Arch
    INCLUDES "${dac_dma_model_config}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_dac_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_dac_dma_test)
target_compile_definitions(nexus_stm32_dac_dma_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_dac_dma_model_privileged
    nx_arch_in_isr=nx_dac_dma_model_isr
    nx_arch_irq_is_masked=nx_dac_dma_model_masked
    nx_arch_irq_save=nx_dac_dma_model_save
    nx_arch_irq_restore=nx_dac_dma_model_restore
    nx_arch_dsb=nx_dac_dma_model_dsb)
target_compile_options(nexus_stm32_dac_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_dac_dma_model.h")
nexus_google_test(nexus_stm32_dac_test STM32F4_LL
    SOURCES stm32_dac_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/dac.c"
    LIBRARIES Nexus::IODAC Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_dac_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_dac_test)
target_compile_definitions(nexus_stm32_dac_test PRIVATE STM32F407xx
    nx_arch_is_privileged=nx_dac_model_privileged
    nx_arch_in_isr=nx_dac_model_isr
    nx_arch_irq_is_masked=nx_dac_model_masked
    nx_arch_dsb=nx_dac_model_dsb)
target_compile_options(nexus_stm32_dac_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_dac_model.h")

# Each density runs a real static factory and the production Platform lifecycle.
foreach(dac_density ve zg)
    set(dac_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-dac-${dac_density}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}"
        --assembly "${NEXUS_SOURCE_DIR}/tests/contracts/dac_assembly/${dac_density}-direct.toml"
        --output "${dac_bundle}" COMMAND_ERROR_IS_FATAL ANY)
    nexus_google_configuration_inputs("${dac_bundle}")
    set(dac_target "nexus_stm32_dac_${dac_density}_factory_test")
    nexus_google_test(${dac_target} STM32F4_LL
        SOURCES stm32_dac_factory_test.cpp "${dac_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/dac.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
        LIBRARIES Nexus::IO
        INCLUDES "${dac_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_include_directories(${dac_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    if(dac_density STREQUAL "ve")
        set(dac_maximum 4095)
        set(dac_scale 1)
    else()
        set(dac_maximum 255)
        set(dac_scale 16)
    endif()
    target_compile_definitions(${dac_target} PRIVATE STM32F407xx
        NX_TEST_DAC_MAX=${dac_maximum} NX_TEST_DAC_SCALE=${dac_scale}
        _init=nx_dac_fixture_init _fini=nx_dac_fixture_fini)
    target_compile_options(${dac_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_control_cold_model.h"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_dac_model.h")
endforeach()

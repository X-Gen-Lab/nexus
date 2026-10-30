# Production HAL transports execute unchanged vendor C with explicit host ports.
# Fixed MMIO mappings belong to Linux contract tests, never firmware consumers.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(hal_contract "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f407_uart_hal")
    set(f407_private "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
    nexus_google_test(nexus_stm32_uart_hal_test
        SOURCES "${hal_contract}/hal_contract_test.cpp"
            "${hal_contract}/cpu_model.cpp"
            "${hal_contract}/dma_flags_port.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_hal.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_cold.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/system.c"
        LIBRARIES Nexus::IO
        INCLUDES "${f407_private}" "${NEXUS_SOURCE_DIR}/arch/include"
            "${CMAKE_CURRENT_SOURCE_DIR}")
    nexus_google_test(nexus_stm32_hal_atomic_test
        SOURCES "${hal_contract}/atomic_contract_test.cpp"
            "${hal_contract}/atomic_vendor_port.c"
        INCLUDES "${f407_private}" "${NEXUS_SOURCE_DIR}/arch/include"
            "${CMAKE_CURRENT_SOURCE_DIR}")
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
    foreach(target nexus_stm32_uart_hal_test nexus_stm32_hal_atomic_test)
        nexus_stm32f407_uart_hal(${target})
        target_link_options(${target} PRIVATE -Wl,--gc-sections)
        nexus_use_cmsis_core(${target})
        if(target STREQUAL "nexus_stm32_uart_hal_test")
            set(model "${hal_contract}/model.h")
        else()
            set(model "${hal_contract}/atomic_model.h")
        endif()
        foreach(unit ${target} ${target}_stm32f407_hal)
            target_compile_features(${unit} PRIVATE c_std_11)
            target_compile_definitions(${unit} PRIVATE
                STM32F407xx NEXUS_STM32_MODEL=1)
            target_include_directories(${unit} PRIVATE
                "${f407_private}" "${f407_private}/hal"
                "${NEXUS_SOURCE_DIR}/arch/include"
                "${CMAKE_CURRENT_SOURCE_DIR}")
            target_include_directories(${unit} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            # External CPU/MMIO declarations precede the target-private profile.
            # The profile is installed last, after the host model defaults.
            target_compile_options(${unit} BEFORE PRIVATE "-include${model}")
            nexus_use_cmsis_host_model(${unit})
        endforeach()
        target_compile_options(${target}_stm32f407_hal PRIVATE
            -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast)
    endforeach()
    # The unchanged SDK flag helpers encode 32-bit silicon addresses. Every
    # mapped hardware address fits that domain in this 64-bit host model.
    foreach(target nexus_stm32_uart_hal_test nexus_stm32_hal_atomic_test)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-pointer-to-int-cast>")
    endforeach()
endif()

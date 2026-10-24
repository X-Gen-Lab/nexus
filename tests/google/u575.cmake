# Real owned U575 functions; only CPU/MMIO/NVIC leaves are modeled.
foreach(u5_suite system provider)
    set(u5_target "nexus_u575_${u5_suite}_test")
    nexus_google_test(${u5_target}
        SOURCES "u575_${u5_suite}_test.cpp" u575_model.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_include_directories(${u5_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(${u5_target} PRIVATE STM32U575xx)
    target_compile_options(${u5_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/google/u575_model.h")
endforeach()

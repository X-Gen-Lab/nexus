# Actual owned G474 startup/providers against external register boundaries.
foreach(g4_suite system provider)
    set(g4_target "nexus_g474_${g4_suite}_test")
    nexus_google_test(${g4_target}
        SOURCES "g474_${g4_suite}_test.cpp"
            "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/system.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/uart.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32g474/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts")
    target_include_directories(${g4_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(${g4_target} PRIVATE STM32G474xx)
    target_compile_options(${g4_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.h")
endforeach()

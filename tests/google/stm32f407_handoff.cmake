# Real C handoff/domain algorithms; only register/instruction operations mocked.
foreach(density 524288 1048576)
    set(name "nexus_stm32f407_handoff_${density}_test")
    nexus_google_test(${name} SOURCES stm32f407_handoff_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/handoff.c"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c"
        INCLUDES
            "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32f407_handoff/include"
            "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
            "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/core/include"
            "${NEXUS_SOURCE_DIR}/tests/google")
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(${name} PRIVATE STM32F407xx
        NX_F407_HANDOFF_MODEL NX_F407_HANDOFF_FLASH_BYTES=${density})
    target_compile_options(${name} PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-fexceptions>")
endforeach()

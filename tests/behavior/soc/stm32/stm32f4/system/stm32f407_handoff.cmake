# Real C handoff/domain algorithms; only register/instruction operations mocked.
foreach(density 524288 1048576)
    set(name "nexus_stm32f407_handoff_${density}_test")
    nexus_google_test(${name} SOURCES "${nexus_google_root}/soc/stm32/stm32f4/system/stm32f407_handoff_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/handoff.c"
        "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/src/clock_domain.c"
        INCLUDES
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
            "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/include"
            "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/private"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/core/include"
            "${nexus_google_root}/soc/stm32/stm32f4/system"
    )
    target_include_directories(
        ${name}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
    )
    nexus_use_cmsis_core(${name})
    target_compile_definitions(
        ${name}
        PRIVATE
            STM32F407xx
            NX_F407_HANDOFF_MODEL
            NX_F407_HANDOFF_FLASH_BYTES=${density}
    )
    target_compile_options(
        ${name}
        PRIVATE "$<$<COMPILE_LANGUAGE:C>:-fexceptions>"
    )
endforeach()

# The real selected orchestration; hardware/source ports are tested separately.
nexus_google_test(nexus_stm32_freertos_stop_test
    SOURCES "${nexus_google_root}/os/freertos_stop_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system/freertos_stop.c"
        "${NEXUS_SOURCE_DIR}/os/freertos/lowpower.c"
        "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/src/clock_domain.c"
    LIBRARIES Nexus::Core
    INCLUDES "${nexus_google_root}/os/lowpower_profile"
        "${NEXUS_SOURCE_DIR}/os/freertos/include"
        "${NEXUS_SOURCE_DIR}/os/freertos/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/include"
        "${NEXUS_SOURCE_DIR}/components/runtime/clock_domain/private"
        "${NEXUS_SOURCE_DIR}/io/include"
        "${NEXUS_SOURCE_DIR}/arch/include"
)
target_include_directories(
    nexus_stm32_freertos_stop_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix"
)

# Real SoC bootstrap algorithm; only MMIO, CPU and physical RAM leaves modeled.
nexus_google_test(nexus_u575_security_test SOURCES u575_security_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/security.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/core/include"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_security_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_security_test PRIVATE STM32U575xx
    __ARM_FEATURE_CMSE=3 NX_U575_SECURITY_MODEL)

nexus_google_test(nexus_u575_secure_batch_test
    SOURCES u575_secure_batch_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/secure_batch.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private")

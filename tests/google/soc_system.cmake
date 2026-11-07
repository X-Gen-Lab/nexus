
nexus_google_test(nexus_stm32_system_transaction_test
    SOURCES stm32_system_transaction_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_system_transaction_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_system_transaction_test)
target_compile_definitions(nexus_stm32_system_transaction_test PRIVATE
    STM32F407xx NEXUS_STM32_MODEL)
target_compile_options(nexus_stm32_system_transaction_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_system_model.h")

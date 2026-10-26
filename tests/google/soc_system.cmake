# Live epochs execute the production timer against external SDK/clock mocks.
nexus_google_test(nexus_gd32_timebase_contract_test
    SOURCES gd32_timebase_contract_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/timebase.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
set(system_gd_firmware
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
target_include_directories(nexus_gd32_timebase_contract_test SYSTEM PRIVATE
    "${system_gd_firmware}/CMSIS/GD/GD32F4xx/Include"
    "${system_gd_firmware}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_timebase_contract_test PRIVATE
    GD32F470 HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_timebase_contract_test PRIVATE
    -Wno-int-to-pointer-cast
    "-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_timebase_model.h")

nexus_google_test(nexus_stm32_system_transaction_test
    SOURCES stm32_system_transaction_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/system.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_system_transaction_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_system_transaction_test)
target_compile_definitions(nexus_stm32_system_transaction_test PRIVATE
    STM32F407xx NEXUS_STM32_MODEL)
target_compile_options(nexus_stm32_system_transaction_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_system_model.h")

nexus_google_test(nexus_gd32_system_transaction_test
    SOURCES gd32_system_transaction_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/system.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_gd32_system_transaction_test SYSTEM PRIVATE
    "${system_gd_firmware}/CMSIS/GD/GD32F4xx/Include"
    "${system_gd_firmware}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_system_transaction_test PRIVATE
    GD32F470 HXTAL_VALUE=25000000 NX_GD32_CLOCK_MODEL)
target_compile_options(nexus_gd32_system_transaction_test PRIVATE
    -Wno-int-to-pointer-cast
    "-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_system_model.h")

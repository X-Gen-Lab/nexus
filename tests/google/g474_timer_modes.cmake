# Real G474 timer algorithms with external register and CPU ports modeled.
nexus_google_test(nexus_g474_timer_modes_test SOURCES g474_timer_modes_test.cpp
    "${NEXUS_SOURCE_DIR}/io/src/api/timer_capture.c"
    "${NEXUS_SOURCE_DIR}/io/src/api/encoder.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/timer_capture.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/encoder.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/include"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_g474_timer_modes_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_timer_modes_test)
target_compile_definitions(nexus_g474_timer_modes_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_timer_modes_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_timer_modes_model.h")

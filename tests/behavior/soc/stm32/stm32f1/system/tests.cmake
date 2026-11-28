# F103 time runs independently of the kernel and retains failed receipts.
# Author: Nexus Team
set(system_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(nexus_stm32f1_timebase_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/timebase_test.cpp"
        "${system_package}/system/platform.c"
    LIBRARIES Nexus::Arch Nexus::Core
    INCLUDES "${system_package}"
)
target_include_directories(
    nexus_stm32f1_timebase_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
nexus_use_cmsis_core(nexus_stm32f1_timebase_test)
target_compile_definitions(nexus_stm32f1_timebase_test PRIVATE STM32F103xB)
target_compile_options(
    nexus_stm32f1_timebase_test
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/register_model.h"
)

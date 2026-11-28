# Test the exact irreversible watchdog witness against the production C.
set(f1_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
nexus_google_test(
    f103_wwdg_ewi_model
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/early_warning_test.cpp"
        "${f1_dir}/drivers/watchdog/early_warning.c"
        "${f1_dir}/drivers/watchdog/window_watchdog.c"
    INCLUDES "${f1_dir}"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOWatchdog
)
nexus_use_cmsis_core(f103_wwdg_ewi_model)
target_include_directories(
    f103_wwdg_ewi_model
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(f103_wwdg_ewi_model PRIVATE STM32F103xB)
target_compile_options(
    f103_wwdg_ewi_model
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)

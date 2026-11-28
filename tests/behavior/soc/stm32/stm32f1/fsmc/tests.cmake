set(f1_fsmc_ll
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Src/stm32f1xx_ll_fsmc.c"
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # ST's unsigned long masks are 32-bit on MCU and 64-bit on this host.
    set_property(
        SOURCE "${f1_fsmc_ll}"
        APPEND
        PROPERTY COMPILE_OPTIONS -Wno-conversion
    )
endif()
nexus_google_test(
    nexus_stm32f1_fsmc_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/fsmc_test.cpp"
        "${f1_dir}/drivers/fsmc/fsmc.c"
        "${f1_dir}/drivers/gpio/gpio.c"
        "${f1_fsmc_ll}"
    INCLUDES
        "${f1_dir}"
        "${f1_dir}/sdk/hal_config/fsmc"
        "${NEXUS_SOURCE_DIR}/arch/include"
    LIBRARIES Nexus::IOGPIO Nexus::IOExternalBus Nexus::IOMemoryWindow
)
nexus_google_test(
    nexus_stm32f1_fsmc_lcd_test
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/lcd_test.cpp"
        "${f1_dir}/drivers/fsmc/fsmc.c"
        "${f1_dir}/drivers/fsmc/lcd.c"
        "${f1_dir}/drivers/gpio/gpio.c"
        "${f1_fsmc_ll}"
    INCLUDES
        "${f1_dir}"
        "${f1_dir}/sdk/hal_config/fsmc"
        "${NEXUS_SOURCE_DIR}/arch/include"
    LIBRARIES Nexus::IOGPIO Nexus::IOParallel16
)
target_compile_definitions(
    nexus_stm32f1_fsmc_lcd_test
    PRIVATE NX_STM32F1_FSMC_LCD=1
)
foreach(model IN ITEMS nexus_stm32f1_fsmc_test nexus_stm32f1_fsmc_lcd_test)
    nexus_use_cmsis_core(${model})
    target_include_directories(
        ${model}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(${model} PRIVATE STM32F103xG)
    target_compile_options(
        ${model}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
    )
endforeach()

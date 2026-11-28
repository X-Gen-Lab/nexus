# Run the selected production C implementation against the Flash bus model.
function(nexus_f1_flash_contract target device)
    nexus_google_test(${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/flash_test.cpp"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/drivers/flash/flash.c"
        LIBRARIES Nexus::IOFlash Nexus::Core
        INCLUDES
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1"
            "${NEXUS_SOURCE_DIR}/arch/include"
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
    )
    target_compile_definitions(${target} PRIVATE ${device})
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/flash_model.h"
    )
endfunction()

nexus_f1_flash_contract(nexus_stm32f1_flash_test STM32F103xB)
nexus_f1_flash_contract(nexus_stm32f1_flash_ze_test STM32F103xE)
nexus_f1_flash_contract(nexus_stm32f1_flash_zg_test STM32F103xG)

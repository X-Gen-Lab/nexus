# Whole enhanced-frame contracts and actual selected production mechanisms.
nexus_google_test(nexus_ethernet_frames_api_test
    SOURCES ethernet_frames_api_test.cpp LIBRARIES Nexus::IO)
foreach(eth_frames_family common stm32 gd32)
    set(eth_frames_target "nexus_${eth_frames_family}_ethernet_frames_test")
    nexus_google_test(${eth_frames_target}
        SOURCES ethernet_frames_mac_test.cpp
            "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_frames_mac.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${eth_frames_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_frames_model.h")
    if(eth_frames_family STREQUAL "stm32")
        target_compile_definitions(${eth_frames_target} PRIVATE
            NX_ETH_FRAMES_TEST_STM32 STM32F407xx USE_RTOS=0
            USE_HAL_ETH_REGISTER_CALLBACKS=0)
        target_sources(${eth_frames_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/ethernet_frames.c")
        target_include_directories(${eth_frames_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
        target_include_directories(${eth_frames_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${eth_frames_target})
        nexus_stm32f4_official(${eth_frames_target})
    elseif(eth_frames_family STREQUAL "gd32")
        target_compile_definitions(${eth_frames_target} PRIVATE
            NX_ETH_FRAMES_TEST_GD32 GD32F470 HXTAL_VALUE=25000000)
        set(eth_frames_gd
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_sources(${eth_frames_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/ethernet_frames.c")
        target_include_directories(${eth_frames_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${eth_frames_target} SYSTEM PRIVATE
            "${eth_frames_gd}/CMSIS/GD/GD32F4xx/Include"
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_options(${eth_frames_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
nexus_google_test(nexus_ethernet_frames_native_test
    SOURCES ethernet_frames_native_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/native/drivers/ethernet_frames.c"
        "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_frames_mac.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
target_compile_definitions(nexus_ethernet_frames_native_test PRIVATE
    NX_ETH_FRAMES_NATIVE)
target_compile_options(nexus_ethernet_frames_native_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_frames_native_handoff_model.h")

# Production public contracts, common MAC, two SDK leaves and explicit PHY.
nexus_google_test(nexus_ethernet_api_test SOURCES ethernet_api_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_ethernet_mac_test SOURCES ethernet_mac_test.cpp
    "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_mac.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
target_compile_options(nexus_ethernet_mac_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_model.h")

foreach(eth_family stm32 gd32)
    set(eth_target "nexus_${eth_family}_ethernet_test")
    nexus_google_test(${eth_target} SOURCES ethernet_mac_test.cpp
        "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_mac.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${eth_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_model.h")
    if(eth_family STREQUAL "stm32")
        target_sources(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/ethernet.c")
        target_include_directories(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
        target_include_directories(${eth_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${eth_target} PRIVATE
            NX_ETH_TEST_STM32 STM32F407xx)
    else()
        set(eth_gd
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_sources(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/ethernet.c")
        target_include_directories(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${eth_target} SYSTEM PRIVATE
            "${eth_gd}/CMSIS/GD/GD32F4xx/Include"
            "${eth_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${eth_target} PRIVATE
            NX_ETH_TEST_GD32 GD32F470 HXTAL_VALUE=25000000)
        target_compile_options(${eth_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()

nexus_google_test(nexus_ethernet_native_test SOURCES ethernet_native_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
nexus_google_test(nexus_dp83848_test SOURCES dp83848_test.cpp
    LIBRARIES Nexus::DP83848 Nexus::IO)

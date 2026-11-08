# D2 proposed initial real runtime RED against unchanged raw production.
nexus_google_test(nexus_ethernet_raw_errata_test SOURCES ethernet_raw_errata_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
target_compile_options(nexus_ethernet_raw_errata_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_model.h")

foreach(eth_family stm32)
    set(eth_target "nexus_${eth_family}_ethernet_raw_errata_test")
    nexus_google_test(${eth_target} SOURCES ethernet_raw_errata_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_mac.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${eth_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_model.h")
    target_sources(${eth_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ethernet.c")
    target_include_directories(${eth_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
    target_include_directories(${eth_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${eth_target})
    target_compile_definitions(${eth_target} PRIVATE
        NX_ETH_TEST_STM32 STM32F407xx)
endforeach()


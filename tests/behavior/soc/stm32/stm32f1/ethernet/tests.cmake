# Exercise the private F107 management transaction and cold receipts.
# Author: Nexus Team
nexus_google_test(
    stm32f107_mdio
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/mdio_test.cpp"
        "${f1_dir}/drivers/ethernet/mdio.c"
        "${f1_dir}/drivers/gpio/gpio.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOMDIO
)
nexus_use_cmsis_core(stm32f107_mdio)
target_include_directories(
    stm32f107_mdio
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f107_mdio
    PRIVATE STM32F107xC NEXUS_CLOCK_SOURCE_HSE=1 NEXUS_CLOCK_HSE_BYPASS=0
)
target_compile_options(
    stm32f107_mdio
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/mdio_model.h"
)

# Exercise the shared normal descriptors with F107 reset-based ownership.
set(raw_model "${nexus_google_root}/soc/stm32/stm32f4/ethernet")
set(raw_dwmac "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwmac")
nexus_google_test(nexus_stm32f107_raw_mac_test
    SOURCES "${raw_model}/ethernet_mac_test.cpp" "${raw_dwmac}/eth_mac.c"
    LIBRARIES Nexus::IOEthernet Nexus::Arch
    INCLUDES "${raw_dwmac}" "${NEXUS_SOURCE_DIR}/io/private"
)
target_compile_definitions(
    nexus_stm32f107_raw_mac_test
    PRIVATE NX_ETH_RESET_RETIRE=1
)
target_compile_options(
    nexus_stm32f107_raw_mac_test
    PRIVATE "-include${raw_model}/ethernet_model.h"
)

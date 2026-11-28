# Compile station management only when the exact assembly claims the MAC.
# Author: Nexus Team
nexus_controller_modes(mdio_modes mdio)
if("clause22" IN_LIST mdio_modes)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/mdio.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOMDIO)
endif()
nexus_controller_modes(raw_modes eth)
if("raw-mii" IN_LIST raw_modes OR "raw-rmii" IN_LIST raw_modes)
    set(dwmac "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwmac")
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/raw.c" "${dwmac}/eth_mac.c"
    )
    target_include_directories(nexus_soc_stm32f1 PRIVATE "${dwmac}")
    target_compile_definitions(nexus_soc_stm32f1 PRIVATE NX_ETH_RESET_RETIRE=1)
    if("raw-rmii" IN_LIST raw_modes)
        target_compile_definitions(
            nexus_soc_stm32f1
            PRIVATE NX_STM32F1_ETH_RMII=1
        )
    endif()
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOEthernet)
endif()

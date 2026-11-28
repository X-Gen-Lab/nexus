# Selected ethernet sources and private dependencies.
nexus_controller_modes(eth_modes eth)
nexus_controller_modes(eth_frames_modes eth_frames)
if(eth_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/ethernet.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwmac/eth_mac.c"
    )
endif()
if(eth_frames_modes)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/ethernet_frames.c"
            "${CMAKE_CURRENT_LIST_DIR}/../ip/legacy_mac/eth_frames_mac.c"
    )
endif()

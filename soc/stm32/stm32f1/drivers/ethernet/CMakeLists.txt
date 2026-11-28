# Compile station management only when the exact assembly claims the MAC.
# Author: Nexus Team
nexus_controller_modes(mdio_modes mdio)
if("clause22" IN_LIST mdio_modes)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/mdio.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOMDIO)
endif()

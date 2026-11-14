# Selected crc sources and private dependencies.
nexus_controller_modes(crc_modes crc)
if(crc_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/crc.c")
endif()

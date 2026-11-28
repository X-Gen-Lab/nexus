# Compile the fixed F103 CRC only when selected by the assembly.
# Author: Nexus Team
nexus_controller_modes(crc_modes crc)
if("crc32-words" IN_LIST crc_modes)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/crc.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOCRC)
endif()

# Selected flash sources and private dependencies.
nexus_controller_modes(flash_modes flash)
if(flash_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/flash.c")
endif()

# Selected entropy sources and private dependencies.
nexus_controller_modes(entropy_modes entropy)
if(entropy_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/entropy.c")
endif()

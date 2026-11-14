# Selected watchdog sources and private dependencies.
nexus_controller_modes(watchdog_modes watchdog)
if("independent" IN_LIST watchdog_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/watchdog.c")
endif()
if("window" IN_LIST watchdog_modes)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/window_watchdog.c")
endif()

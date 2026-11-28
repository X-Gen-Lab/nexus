# Select only the watchdog hardware owner requested by the assembly.
# Author: Nexus Team
nexus_controller_modes(watchdog_modes watchdog)
if("independent" IN_LIST watchdog_modes)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/watchdog.c"
    )
endif()
if("window" IN_LIST watchdog_modes)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/window_watchdog.c"
    )
endif()
if("independent" IN_LIST watchdog_modes OR "window" IN_LIST watchdog_modes)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOWatchdog)
endif()

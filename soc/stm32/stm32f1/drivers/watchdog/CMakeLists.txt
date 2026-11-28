# Select the F103xB independent watchdog when declared by the assembly.
# Author: Nexus Team
nexus_controller_modes(watchdog_modes watchdog)
if("independent" IN_LIST watchdog_modes)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/watchdog.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOWatchdog)
endif()

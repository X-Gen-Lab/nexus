# Offer the reset-only witness only on the explicitly authorized exact route.
# Author: Nexus Team
if(
    NOT NEXUS_EXACT_PART STREQUAL "STM32F103RBT6"
    OR NOT NEXUS_BACKEND STREQUAL "baremetal"
)
    return()
endif()
nexus_controller_modes(window_modes watchdog WWDG)
if(NOT "window" IN_LIST window_modes)
    return()
endif()
file(READ "${NEXUS_CONFIG_DIR}/resolved.json" warning_assembly)
string(JSON warning_board GET "${warning_assembly}" board)
string(
    JSON warning_choice
    ERROR_VARIABLE warning_error
    GET "${warning_assembly}"
    hardware
    selected
    backup_word
)
if(
    NOT warning_board STREQUAL "stm32f103rb-nucleo"
    OR warning_error
    OR NOT warning_choice STREQUAL "wwdg-early-warning-dr1"
)
    return()
endif()
string(JSON warning_count LENGTH "${warning_assembly}" controllers)
math(EXPR warning_last "${warning_count} - 1")
foreach(index RANGE ${warning_last})
    string(
        JSON warning_route
        GET "${warning_assembly}"
        controllers
        ${index}
        route
    )
    if(warning_route STREQUAL "wwdg-early-warning")
        add_library(
            nexus_stm32f103rb_watchdog_warning
            STATIC
            EXCLUDE_FROM_ALL
            "${CMAKE_CURRENT_LIST_DIR}/early_warning.c"
            "${CMAKE_CURRENT_LIST_DIR}/early_warning_port.c"
        )
        nexus_apply_target_policy(nexus_stm32f103rb_watchdog_warning)
        target_include_directories(
            nexus_stm32f103rb_watchdog_warning
            PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../include"
        )
        target_link_libraries(
            nexus_stm32f103rb_watchdog_warning
            PUBLIC Nexus::IOWatchdog
            PRIVATE
                nexus_soc_stm32f1
                nexus_stm32f1_startup_usage
                Nexus::Arch
                nexus_build_options
                nexus_owned_warnings
        )
        add_library(
            Nexus::STM32F103RBWatchdogWarning
            ALIAS nexus_stm32f103rb_watchdog_warning
        )
        break()
    endif()
endforeach()

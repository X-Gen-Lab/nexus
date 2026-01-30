##############################################################################
# LoadKconfig.cmake - Kconfig Configuration Loader
##############################################################################
#
# LoadKconfig.cmake
# Kconfig configuration loader for Nexus build system
# Author: Nexus Team
#
# This module reads the .config file and sets CMake variables for each
# configuration option.
#
##############################################################################

##############################################################################
# Kconfig Loading Function
##############################################################################

#
# Load Kconfig configuration from .config file
# CONFIG_FILE: Path to .config file
#
function(load_kconfig CONFIG_FILE)
    if(NOT EXISTS ${CONFIG_FILE})
        message(WARNING "Config file not found: ${CONFIG_FILE}")
        return()
    endif()

    message(STATUS "Loading Kconfig from: ${CONFIG_FILE}")

    # Read the config file
    file(STRINGS ${CONFIG_FILE} CONFIG_LINES)

    # Parse each line
    foreach(LINE ${CONFIG_LINES})
        # Skip comments and empty lines
        if(LINE MATCHES "^#" OR LINE MATCHES "^$")
            continue()
        endif()

        # Parse CONFIG_XXX=y format
        if(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=y$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} TRUE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} TRUE CACHE BOOL "Kconfig option" FORCE)
        # Parse CONFIG_XXX=n format
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=n$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} FALSE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} FALSE CACHE BOOL "Kconfig option" FORCE)
        # Parse CONFIG_XXX="value" format (string)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=\"([^\"]*)\"$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} "${VAR_VALUE}" PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} "${VAR_VALUE}" CACHE STRING "Kconfig option" FORCE)
        # Parse CONFIG_XXX=value format (number or hex)
        elseif(LINE MATCHES "^CONFIG_([A-Za-z0-9_]+)=([0-9x]+)$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(VAR_VALUE ${CMAKE_MATCH_2})
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} ${VAR_VALUE} CACHE STRING "Kconfig option" FORCE)
        # Parse # CONFIG_XXX is not set format
        elseif(LINE MATCHES "^# CONFIG_([A-Za-z0-9_]+) is not set$")
            set(VAR_NAME ${CMAKE_MATCH_1})
            set(CONFIG_${VAR_NAME} FALSE PARENT_SCOPE)
            set(CONFIG_${VAR_NAME} FALSE CACHE BOOL "Kconfig option" FORCE)
        endif()
    endforeach()

    message(STATUS "Loaded Kconfig configuration")
endfunction()

##############################################################################
# End of LoadKconfig.cmake
##############################################################################

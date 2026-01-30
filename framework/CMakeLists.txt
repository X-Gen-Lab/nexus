##############################################################################
# Framework Layer
##############################################################################

message(STATUS "Configuring Framework Layer...")

#-----------------------------------------------------------------------------
# Core Framework Components
#-----------------------------------------------------------------------------

# Log framework - Logging system
add_subdirectory(log)

# Shell framework - Command-line interface
add_subdirectory(shell)

# Config Manager framework - Configuration management
add_subdirectory(config)

# Init framework - System initialization
add_subdirectory(init)

#-----------------------------------------------------------------------------
# Optional Framework Components
#-----------------------------------------------------------------------------

# Event framework (future)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/event AND NEXUS_ENABLE_EVENT_FRAMEWORK)
    add_subdirectory(event)
    message(STATUS "Event Framework: Enabled")
endif()

# State Machine framework (future)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/state_machine AND NEXUS_ENABLE_STATE_MACHINE)
    add_subdirectory(state_machine)
    message(STATUS "State Machine Framework: Enabled")
endif()

# Middleware framework (future)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/middleware AND NEXUS_ENABLE_MIDDLEWARE)
    add_subdirectory(middleware)
    message(STATUS "Middleware Framework: Enabled")
endif()

message(STATUS "Framework Layer: Configured")

#-----------------------------------------------------------------------------
# End of Framework Layer
#-----------------------------------------------------------------------------

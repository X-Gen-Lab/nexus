##############################################################################
# Nexus Example Applications
##############################################################################

# Only build blinky for embedded platforms
if(NOT NEXUS_PLATFORM STREQUAL "native")
    add_subdirectory(blinky)
endif()

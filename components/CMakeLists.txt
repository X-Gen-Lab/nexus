# Explicit optional component groups; leaf targets own their dependencies.
# Author: Nexus Team
add_library(nexus_component_bindings INTERFACE)
add_library(nexus_component_api INTERFACE)
add_subdirectory(algorithms)
add_subdirectory(control)
add_subdirectory(runtime)
add_subdirectory(buses)
add_subdirectory(devices)
add_subdirectory(services)
add_subdirectory(protocols)

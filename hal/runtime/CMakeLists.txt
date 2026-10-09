# Completion dispatch is independent of devices, OSAL and worker ownership.
add_library(hal_completion STATIC nx_completion.c)
target_link_libraries(hal_completion PUBLIC hal_interface PRIVATE nexus_arch)
add_library(Nexus::HALCompletion ALIAS hal_completion)

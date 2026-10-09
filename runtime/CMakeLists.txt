add_library(nexus_runtime STATIC src/nx_runtime.c src/nx_platform_info.c)
target_include_directories(nexus_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_runtime PUBLIC hal_core hal_support osal)
add_library(Nexus::Runtime ALIAS nexus_runtime)

# Explicit object injection preserves registration, strong IRQ/callback symbols
# and selected startup. Components remain explicit application dependencies.
add_library(nexus_firmware INTERFACE)
target_link_libraries(nexus_firmware INTERFACE
    "platform_${NEXUS_PLATFORM}"
    "$<TARGET_OBJECTS:platform_${NEXUS_PLATFORM}>" nexus_runtime hal_device)
add_library(Nexus::Firmware ALIAS nexus_firmware)

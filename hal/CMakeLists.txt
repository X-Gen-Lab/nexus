# Public contracts carry only Nexus configuration and standard C types.
add_library(hal_interface INTERFACE)
target_include_directories(hal_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(hal_interface INTERFACE nexus_build_options)
add_library(hal_provider_interface INTERFACE)
target_link_libraries(hal_provider_interface INTERFACE hal_interface)
add_library(Nexus::HALProviders ALIAS hal_provider_interface)

# Registry/ownership never links OSAL, vendor SDKs or Board implementations.
add_library(hal_core STATIC src/nx_device.c src/nx_status.c src/nx_error.c src/nx_assert.c)
target_include_directories(hal_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(hal_core PUBLIC hal_interface PRIVATE hal_provider_interface nexus_arch)
add_library(Nexus::HALCore ALIAS hal_core)

# Independent typed facades. A consumer can link one without other facade code.
foreach(facade gpio uart spi i2c flash)
    add_library(hal_${facade} STATIC "src/nx_device_${facade}.c")
    target_include_directories(hal_${facade} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(hal_${facade} PUBLIC hal_core PRIVATE hal_provider_interface nexus_arch)
endforeach()
add_library(Nexus::HALGPIO ALIAS hal_gpio)
add_library(Nexus::HALUART ALIAS hal_uart)
add_library(Nexus::HALSPI ALIAS hal_spi)
add_library(Nexus::HALI2C ALIAS hal_i2c)
add_library(Nexus::HALFlash ALIAS hal_flash)
add_library(hal_device INTERFACE)
target_link_libraries(hal_device INTERFACE hal_gpio hal_uart hal_spi hal_i2c hal_flash)
add_library(Nexus::HALDevice ALIAS hal_device)

# OSAL-backed support is explicit, separate from typed identity and I/O leases.
add_library(hal_support STATIC src/nx_hal.c src/nx_mutex.c src/nx_mem.c src/nx_power_manager.c)
target_link_libraries(hal_support PUBLIC hal_interface PRIVATE hal_core osal nexus_arch)
add_library(Nexus::HALSupport ALIAS hal_support)
add_library(hal INTERFACE)
target_link_libraries(hal INTERFACE hal_device hal_support)

# Generic waits are caller-driven; the optional OSAL binding is a separate port.
add_library(hal_runtime STATIC src/nx_deadline.c)
target_link_libraries(hal_runtime PUBLIC hal_uart PRIVATE nexus_arch)
add_library(Nexus::HALRuntime ALIAS hal_runtime)
add_library(hal_runtime_osal STATIC src/nx_deadline_osal.c)
target_link_libraries(hal_runtime_osal PUBLIC hal_runtime PRIVATE osal)
add_library(Nexus::HALRuntimeOSAL ALIAS hal_runtime_osal)

# Explicit caller-owned terminal dispatch, with no default OSAL dependency.
add_subdirectory(runtime)

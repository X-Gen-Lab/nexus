# Explicit software-reference package; not part of the production SoC catalogue.
if(NOT NEXUS_TEST_SUPPORT)
    message(FATAL_ERROR "The GD32 reference requires explicit test support")
endif()
# The sidecar remains outside the core SDK identity and must be supplied explicitly.
if(NOT NEXUS_GD32_VENDOR_ROOT OR NOT IS_DIRECTORY "${NEXUS_GD32_VENDOR_ROOT}")
    message(FATAL_ERROR "GD32 supplier root must be supplied explicitly")
endif()
get_filename_component(gd_supplier "${NEXUS_GD32_VENDOR_ROOT}" ABSOLUTE)
file(READ "${CMAKE_CURRENT_LIST_DIR}/references.json" gd_references)
string(JSON gd_lock_name GET "${gd_references}" supplier lock)
string(JSON gd_lock_expected GET "${gd_references}" supplier sha256)
if(NOT gd_lock_name STREQUAL "source.lock.json" OR
        NOT EXISTS "${gd_supplier}/${gd_lock_name}" OR
        IS_SYMLINK "${gd_supplier}/${gd_lock_name}")
    message(FATAL_ERROR "GD32 supplier lock is missing or invalid")
endif()
file(SHA256 "${gd_supplier}/${gd_lock_name}" gd_lock_actual)
if(NOT gd_lock_actual STREQUAL gd_lock_expected)
    message(FATAL_ERROR "GD32 supplier lock identity differs from the declared reference")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/scripts/ci/verify_vendor_source.py" --root "${gd_supplier}"
    RESULT_VARIABLE gd_supplier_status OUTPUT_VARIABLE gd_supplier_output
    ERROR_VARIABLE gd_supplier_error)
if(NOT gd_supplier_status EQUAL 0)
    message(FATAL_ERROR "GD32 supplier payload rejected: ${gd_supplier_output}${gd_supplier_error}")
endif()
file(GLOB_RECURSE gd_supplier_files CONFIGURE_DEPENDS "${gd_supplier}/Firmware/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/references.json" "${gd_supplier}/${gd_lock_name}" ${gd_supplier_files})
if(NEXUS_GD32_VERIFY_ONLY)
    return()
endif()
set(gd_sdk "${gd_supplier}/Firmware")
set(gd_private "${CMAKE_CURRENT_LIST_DIR}")
set(gd_vendor_includes
    "${gd_sdk}/CMSIS/GD/GD32F4xx/Include"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Include")
set(gd_vendor_sources
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_timer.c")
set(gd_sources)
set(gd_gpio_selected 0)
set(gd_uart_selected 0)
if("gpio" IN_LIST NEXUS_SELECTED_KINDS)
    set(gd_gpio_selected 1)
    list(APPEND gd_sources gpio.c)
endif()
if("uart" IN_LIST NEXUS_SELECTED_KINDS)
    set(gd_uart_selected 1)
    list(APPEND gd_sources uart.c)
    list(APPEND gd_vendor_sources
        "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c")
endif()
if(gd_gpio_selected OR gd_uart_selected)
    list(APPEND gd_vendor_sources
        "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c")
endif()
add_library(nexus_gd32_reference_spl STATIC ${gd_vendor_sources})
target_link_libraries(nexus_gd32_reference_spl PRIVATE nexus_build_options)
target_include_directories(nexus_gd32_reference_spl PRIVATE "${gd_private}")
target_include_directories(nexus_gd32_reference_spl SYSTEM PRIVATE ${gd_vendor_includes})
target_compile_definitions(nexus_gd32_reference_spl PRIVATE GD32F470)
nexus_use_cmsis_core(nexus_gd32_reference_spl)
if(gd_sources)
    add_library(nexus_soc_gd32f470_reference STATIC ${gd_sources})
    target_link_libraries(nexus_soc_gd32f470_reference PUBLIC Nexus::IO Nexus::Arch
        PRIVATE nexus_gd32_reference_spl nexus_build_options nexus_owned_warnings)
    target_include_directories(nexus_soc_gd32f470_reference PRIVATE "${gd_private}")
    target_include_directories(nexus_soc_gd32f470_reference SYSTEM PRIVATE ${gd_vendor_includes})
    target_compile_definitions(nexus_soc_gd32f470_reference PRIVATE GD32F470)
    nexus_use_cmsis_core(nexus_soc_gd32f470_reference)
else()
    add_library(nexus_soc_gd32f470_reference INTERFACE)
    target_link_libraries(nexus_soc_gd32f470_reference INTERFACE
        Nexus::IO Nexus::Arch nexus_gd32_reference_spl)
endif()
set(NEXUS_STARTUP_SOURCE
    "${gd_sdk}/CMSIS/GD/GD32F4xx/Source/GCC/startup_gd32f450_470.S" PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${gd_private}/system.c" PARENT_SCOPE)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES "${gd_private}" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${gd_vendor_includes}" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS GD32F470
    "NX_GD32_GPIO_SELECTED=${gd_gpio_selected}"
    "NX_GD32_UART_SELECTED=${gd_uart_selected}" PARENT_SCOPE)

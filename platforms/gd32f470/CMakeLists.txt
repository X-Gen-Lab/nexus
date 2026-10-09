if(NOT CONFIG_GD32F470ZG OR NOT (CONFIG_BOARD_GD32F470_LIANGSHAN OR CONFIG_BOARD_EXTERNAL))
    message(FATAL_ERROR "GD32F470 requires the maintained GD32F470ZGT6 Liangshan board binding")
endif()
if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR NOT CONFIG_CPU_ARCH STREQUAL "cortex-m4")
    message(FATAL_ERROR "GD32F470 requires ARM GCC and Cortex-M4")
endif()
if(NOT CONFIG_GD32_SYSCLK_FREQ EQUAL 200000000 OR NOT CONFIG_GD32_HXTAL_VALUE EQUAL 25000000)
    message(FATAL_ERROR "Liangshan clock binding requires 25 MHz HXTAL and the maintained 200 MHz profile")
endif()
set(firmware "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${NEXUS_SOURCE_DIR}/scripts/ci/verify_vendor_source.py"
    --root "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx"
    RESULT_VARIABLE sdk_verification OUTPUT_VARIABLE sdk_verification_output
    ERROR_VARIABLE sdk_verification_error)
if(NOT sdk_verification EQUAL 0)
    message(FATAL_ERROR "GD32F4xx source lock verification failed: ${sdk_verification_output}${sdk_verification_error}")
endif()
set(cmsis "${firmware}/CMSIS")
set(device "${cmsis}/GD/GD32F4xx")
set(peripheral "${firmware}/GD32F4xx_standard_peripheral")
set(startup_source "${device}/Source/GCC/startup_gd32f450_470.S")
set(linker_script "${NEXUS_GENERATED_LINKER_SCRIPT}")
foreach(required "${cmsis}/core_cm4.h" "${device}/Include/gd32f4xx.h"
        "${startup_source}" "${linker_script}" "${peripheral}/Source/gd32f4xx_rcu.c")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing locked GD32F4xx 3.3.3 SDK file ${required}")
    endif()
endforeach()

# Implementation targets opt into vendor types. The platform exports Nexus
# interfaces only; application SDK access requires explicit Nexus::GD32SDK.
add_library(sdk_gd32f4xx_headers INTERFACE)
target_include_directories(sdk_gd32f4xx_headers INTERFACE
    "${cmsis}" "${device}/Include" "${peripheral}/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_compile_definitions(sdk_gd32f4xx_headers INTERFACE
    GD32F470 HXTAL_VALUE=${CONFIG_GD32_HXTAL_VALUE})
target_link_libraries(sdk_gd32f4xx_headers INTERFACE nexus_build_options)
set(vendor_sources)
foreach(controller rcu gpio usart spi fmc dbg pmu timer)
    list(APPEND vendor_sources "${peripheral}/Source/gd32f4xx_${controller}.c")
endforeach()
add_library(sdk_gd32f4xx STATIC ${vendor_sources})
add_library(Nexus::GD32SDK ALIAS sdk_gd32f4xx)
target_link_libraries(sdk_gd32f4xx PUBLIC sdk_gd32f4xx_headers)

add_subdirectory("${NEXUS_SOURCE_DIR}/soc/gd32f470zg" "${NEXUS_BINARY_DIR}/soc/gd32f470zg")
set(component_objects soc_gd32f470)
nexus_add_selected_board()
foreach(controller gpio uart spi)
    string(TOUPPER "${controller}" symbol)
    if(CONFIG_GD32_${symbol}_ENABLE)
        set(controller_target "controller_gd32f470_${controller}")
        add_library(${controller_target} OBJECT "src/${controller}.c")
        target_include_directories(${controller_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_link_libraries(${controller_target} PRIVATE
            sdk_gd32f4xx Nexus::HALProviders hal_support osal nexus_arch ${NEXUS_BOARD_TARGET})
        list(APPEND component_objects ${controller_target})
    endif()
endforeach()
add_library(platform_gd32f470 OBJECT src/platform.c "${startup_source}")
# Explicit object forwarding retains strong IRQ handlers and device registry
# records; nested OBJECT-library SOURCES do not propagate to final consumers.
foreach(component IN LISTS component_objects)
    target_link_libraries(platform_gd32f470 INTERFACE "$<TARGET_OBJECTS:${component}>")
endforeach()
target_include_directories(platform_gd32f470 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(platform_gd32f470 PRIVATE
    sdk_gd32f4xx Nexus::HALProviders hal_support osal nexus_arch ${NEXUS_BOARD_TARGET})
target_link_libraries(platform_gd32f470 INTERFACE hal osal nexus_arch
    soc_gd32f470_interface ${NEXUS_BOARD_TARGET} "$<LINK_ONLY:sdk_gd32f4xx>")
set_target_properties(platform_gd32f470 PROPERTIES
    NEXUS_STARTUP_SOURCE "${startup_source}"
    NEXUS_LINKER_SCRIPT "${linker_script}"
    NEXUS_SDK_TARGET sdk_gd32f4xx
    NEXUS_COMPONENT_OBJECT_TARGETS "${component_objects}")
target_link_options(platform_gd32f470 INTERFACE "-T${linker_script}")
set_property(TARGET platform_gd32f470 APPEND PROPERTY INTERFACE_LINK_DEPENDS "${linker_script}"
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/gd32f470_sections.ld")

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
foreach(required "${cmsis}/core_cm4.h" "${device}/Include/gd32f4xx.h"
        "${startup_source}" "${peripheral}/Source/gd32f4xx_rcu.c")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing locked GD32F4xx 3.3.3 SDK file ${required}")
    endif()
endforeach()

# Implementation targets opt into vendor types. The platform exports Nexus
# interfaces only; application SDK access requires explicit Nexus::GD32SDK.
add_library(sdk_gd32f4xx_headers INTERFACE)
target_include_directories(sdk_gd32f4xx_headers INTERFACE
    "${cmsis}" "${device}/Include" "${peripheral}/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
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


set(NEXUS_SOC_STARTUP_SOURCE "${startup_source}" PARENT_SCOPE)

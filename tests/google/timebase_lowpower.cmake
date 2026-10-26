# Real optional timer protocol, with only external register/clock/kernel mocks.
nexus_google_test(nexus_timebase_lowpower_test
    SOURCES timebase_lowpower_test.cpp ../../os/freertos/timebase_lowpower.c
    LIBRARIES Nexus::Core
    INCLUDES lowpower_profile ../../os/freertos/private
        ../../os/freertos/include ../../arch/include)
target_include_directories(nexus_timebase_lowpower_test SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/ext/freertos/include"
    "${PROJECT_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
target_compile_options(nexus_timebase_lowpower_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/timebase_lowpower_model.h")

foreach(timebase_family stm32f407 gd32f470)
    set(timebase_target "nexus_${timebase_family}_lowpower_test")
    nexus_google_test(${timebase_target} SOURCES timebase_soc_test.cpp
        "${NEXUS_SOC_SOURCE_${timebase_family}}/timebase_lowpower.c"
        "${NEXUS_SOURCE_DIR}/os/freertos/timebase_lowpower.c"
        LIBRARIES Nexus::Core
        INCLUDES timebase_soc_profile
            "${NEXUS_SOC_SOURCE_${timebase_family}}/private"
            "${NEXUS_SOURCE_DIR}/os/freertos/private"
            "${NEXUS_SOURCE_DIR}/os/freertos/include"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${timebase_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
    target_compile_options(${timebase_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/timebase_soc_model.h")
    if(timebase_family STREQUAL "stm32f407")
        target_include_directories(${timebase_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${timebase_target})
        target_compile_definitions(${timebase_target} PRIVATE STM32F407xx)
    else()
        set(timebase_vendor
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${timebase_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${timebase_target} SYSTEM PRIVATE
            "${timebase_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${timebase_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${timebase_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000 NX_TIMEBASE_GD_MODEL)
        target_compile_options(${timebase_target} PRIVATE -Wno-int-to-pointer-cast)
    endif()
endforeach()

# Execute the selected binding with its real monotonic IRQ and release path.
foreach(timebase_family stm32f407 gd32f470)
    set(timebase_target "nexus_${timebase_family}_lowpower_integration_test")
    if(timebase_family STREQUAL "stm32f407")
        set(timebase_clock_source system.c)
    else()
        set(timebase_clock_source timebase.c)
    endif()
    nexus_google_test(${timebase_target} SOURCES timebase_soc_integration_test.cpp
        "${NEXUS_SOC_SOURCE_${timebase_family}}/${timebase_clock_source}"
        "${NEXUS_SOC_SOURCE_${timebase_family}}/timebase_lowpower.c"
        "${NEXUS_SOURCE_DIR}/os/freertos/timebase_lowpower.c"
        LIBRARIES Nexus::Core
        INCLUDES timebase_soc_profile
            "${NEXUS_SOC_SOURCE_${timebase_family}}/private"
            "${NEXUS_SOC_SOURCE_${timebase_family}}"
            "${NEXUS_SOURCE_DIR}/os/freertos/private"
            "${NEXUS_SOURCE_DIR}/os/freertos/include"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${timebase_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")
    target_compile_definitions(${timebase_target} PRIVATE
        NEXUS_TIMEBASE_LOWPOWER=1 NX_TIMEBASE_INTEGRATION)
    target_compile_options(${timebase_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/timebase_soc_model.h")
    if(timebase_family STREQUAL "stm32f407")
        target_include_directories(${timebase_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${timebase_target})
        target_compile_definitions(${timebase_target} PRIVATE
            STM32F407xx NEXUS_STM32_MODEL)
    else()
        target_include_directories(${timebase_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${timebase_target} SYSTEM PRIVATE
            "${timebase_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${timebase_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${timebase_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000 NX_TIMEBASE_GD_MODEL)
        target_compile_options(${timebase_target} PRIVATE -Wno-int-to-pointer-cast)
    endif()
endforeach()

# Real optional domain mechanism; only external CPU context leaves are modeled.
add_library(nexus_clock_domain_test_provider STATIC
    "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c")
target_include_directories(nexus_clock_domain_test_provider PUBLIC
    "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/private")
target_link_libraries(nexus_clock_domain_test_provider PUBLIC Nexus::Core
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
target_compile_definitions(nexus_clock_domain_test_provider PRIVATE
    nx_arch_is_privileged=nx_clock_test_privileged
    nx_arch_exception_number=nx_clock_test_exception)
nexus_google_test(nexus_clock_domain_test SOURCES clock_domain_test.cpp
    LIBRARIES nexus_clock_domain_test_provider Nexus::Arch)
foreach(sleep_family stm32f407 gd32f470)
    set(sleep_target "nexus_${sleep_family}_clock_sleep_test")
    nexus_google_test(${sleep_target} SOURCES clock_sleep_soc_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/${sleep_family}/sleep.c"
        "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_sleep.c"
        LIBRARIES nexus_clock_domain_test_provider Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/${sleep_family}/private"
            "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${sleep_target} PRIVATE NEXUS_ARCH_SLEEP_MODEL)
    target_compile_options(${sleep_target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/clock_sleep_soc_model.h>")
    if(sleep_family STREQUAL "stm32f407")
        target_include_directories(${sleep_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    else()
        set(sleep_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${sleep_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/private")
        target_include_directories(${sleep_target} SYSTEM PRIVATE
            "${sleep_vendor}/CMSIS"
            "${sleep_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${sleep_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${sleep_target} PRIVATE GD32F470
            NX_CLOCK_GD_MODEL HXTAL_VALUE=25000000)
        target_compile_options(${sleep_target} PRIVATE -Wno-int-to-pointer-cast)
    endif()
endforeach()

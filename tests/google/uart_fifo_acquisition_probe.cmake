# Internal fixture tests mock observation leaves and the real IRQ dependency.
# They do not test a replacement provider, nor prove linked ARM interception.
set(fifo_fixture
    "${NEXUS_SOURCE_DIR}/tests/contracts/uart_fifo_acquisition")
foreach(family IN ITEMS h743 u575)
    if(family STREQUAL h743)
        set(soc stm32h743)
        set(sdk h7)
        set(device STM32H743xx)
        set(selection FIFO_FIXTURE_H743=1)
    else()
        set(soc stm32u575)
        set(sdk u5)
        set(device STM32U575xx)
        set(selection FIFO_FIXTURE_U575=1)
    endif()
    set(target "nexus_${family}_fifo_acquisition_probe_test")
    nexus_google_test(${target}
        SOURCES uart_fifo_acquisition_probe_test.cpp
            "${fifo_fixture}/irq_probe.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOC_SOURCE_${soc}}/private")
    target_compile_definitions(${target} PRIVATE ${selection} ${device}
        FIFO_FIXTURE_REVIEWED_MARKERS=1)
    # Reuse the existing host-only device-header boundary. Firmware still uses
    # its generated exact policy and complete CMSIS helpers.
    target_compile_options(${target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/${family}_model.h")
    target_include_directories(${target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_${sdk}/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
    nexus_stm32_official(${target} ${sdk})
endforeach()

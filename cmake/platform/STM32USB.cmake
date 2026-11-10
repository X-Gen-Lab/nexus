# Selected official USB transports have one private callback ABI and LL owner.
include_guard(GLOBAL)

function(nexus_stm32f407_usb_hal target)
    cmake_parse_arguments(USB "HOST" "" "" ${ARGN})
    if(NOT TARGET "${target}" OR USB_UNPARSED_ARGUMENTS OR
            NOT USB_HOST)
        message(FATAL_ERROR "USB HAL requires one owned Host target")
    endif()
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/STM32Official.cmake")
    nexus_stm32f4_device_definition("${target}" device)
    nexus_stm32f4_official("${target}")
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(vendor "${target}_usb_hal")
    if(TARGET "${vendor}")
        message(FATAL_ERROR "USB HAL transport is already selected")
    endif()
    set(sources "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_usb.c")
    if(USB_HOST)
        list(APPEND sources
            "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_hcd.c")
    endif()
    add_library("${vendor}" OBJECT ${sources})
    nexus_stm32f4_official("${vendor}")
    target_compile_features("${vendor}" PRIVATE c_std_11)
    set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions("${vendor}" PRIVATE "${device}")
    target_include_directories("${vendor}" PRIVATE
        "${source}/soc/stm32/stm32f4/private/usb_hal")
    target_include_directories("${vendor}" SYSTEM PRIVATE
        "${source}/vendors/st/cmsis_device_f4/Include")
    include("${source}/cmake/platform/CMSISCore.cmake")
    nexus_use_cmsis_core("${vendor}")
    target_link_libraries("${vendor}" PRIVATE nexus_build_options Nexus::Arch)
    target_compile_options("${vendor}" PRIVATE
        -ffunction-sections -fdata-sections)
    target_sources("${target}" PRIVATE "$<TARGET_OBJECTS:${vendor}>"
        "${source}/soc/stm32/stm32f4/drivers/usb_hal_time.c")
endfunction()

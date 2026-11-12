# Selected stateless official FMC SDRAM leaves, private to one owned target.
include_guard(GLOBAL)

function(nexus_stm32f4_sdram_ll target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}")
        message(FATAL_ERROR "STM32F4 SDRAM requires one owned compile target")
    endif()
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/STM32Official.cmake")
    nexus_stm32f4_device_definition("${target}" device)
    if(NOT device STREQUAL "STM32F479xx")
        message(FATAL_ERROR "SDRAM currently requires the exact F479 selection")
    endif()
    nexus_stm32f4_official("${target}")
    set(vendor "${target}_sdram_ll")
    if(TARGET "${vendor}")
        message(FATAL_ERROR "STM32F4 SDRAM transport already selected")
    endif()
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    add_library("${vendor}" OBJECT
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fmc.c")
    nexus_stm32f4_official("${vendor}")
    target_compile_features("${vendor}" PRIVATE c_std_11)
    set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions("${vendor}" PRIVATE "${device}")
    target_include_directories("${vendor}" PRIVATE
        "${source}/soc/stm32/stm32f4/private/sdram_hal")
    target_include_directories("${vendor}" SYSTEM PRIVATE
        "${source}/vendors/st/cmsis_device_f4/Include")
    include("${source}/cmake/platform/CMSISCore.cmake")
    nexus_use_cmsis_core("${vendor}")
    target_link_libraries("${vendor}" PRIVATE nexus_build_options Nexus::Arch)
    target_compile_options("${vendor}" PRIVATE
        -ffunction-sections -fdata-sections)
    target_sources("${target}" PRIVATE "$<TARGET_OBJECTS:${vendor}>")
endfunction()

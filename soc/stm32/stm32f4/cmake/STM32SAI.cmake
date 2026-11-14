# Official SAI cold configuration; System owns the selected clock plan.
include_guard(GLOBAL)

function(nexus_stm32f4_sai_hal target)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/STM32Official.cmake")
    nexus_stm32f4_device_definition("${target}" device)
    get_filename_component(source
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../.." ABSOLUTE)
    set(vendor "${target}_sai_hal")
    add_library("${vendor}" OBJECT
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_sai.c"
        "${source}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_sai_ex.c")
    nexus_stm32f4_dma_hal("${target}")
    nexus_stm32f4_rcc_hal("${target}" EXTENDED)
    nexus_stm32f4_official("${vendor}")
    target_compile_features("${vendor}" PRIVATE c_std_11)
    set_target_properties("${vendor}" PROPERTIES C_EXTENSIONS OFF)
    target_compile_definitions("${vendor}" PRIVATE "${device}")
    target_include_directories("${vendor}" PRIVATE
        "${source}/soc/stm32/stm32f4/private/sai_hal")
    target_include_directories("${vendor}" SYSTEM PRIVATE
        "${source}/vendors/st/cmsis_device_f4/Include")
    include("${source}/cmake/platform/CMSISCore.cmake")
    nexus_use_cmsis_core("${vendor}")
    target_link_libraries("${vendor}" PRIVATE nexus_build_options Nexus::Arch)
    target_compile_options("${vendor}" PRIVATE
        -ffunction-sections -fdata-sections)
    target_sources("${target}" PRIVATE "$<TARGET_OBJECTS:${vendor}>")
endfunction()

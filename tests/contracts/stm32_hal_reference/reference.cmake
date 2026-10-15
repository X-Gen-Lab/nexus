# One explicit source graph shared by the standalone and Linux Google plans.
include_guard(GLOBAL)
function(nexus_stm32_hal_reference target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}" OR CMAKE_CROSSCOMPILING
       OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "HAL reference requires one Linux host executable")
    endif()
    set(fixture "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    get_filename_component(source "${fixture}/../../.." ABSOLUTE)
    include("${source}/cmake/platform/STM32Official.cmake")
    set(official "${source}/vendors/st/stm32f4_hal")
    set(device "${source}/vendors/st/cmsis_device_f4/Include")
    set(cmsis "${source}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    add_library(${target}_vendor STATIC
        "${official}/Src/stm32f4xx_hal_uart.c"
        "${official}/Src/stm32f4xx_hal_dma.c")
    nexus_stm32f4_official(${target}_vendor)
    nexus_stm32f4_official(${target})
    target_compile_features(${target}_vendor PRIVATE c_std_11)
    target_compile_features(${target} PRIVATE cxx_std_17)
    set_target_properties(${target}_vendor PROPERTIES C_EXTENSIONS OFF)
    foreach(unit ${target}_vendor ${target})
        target_compile_definitions(${unit} PRIVATE STM32F407xx)
        target_include_directories(${unit} PRIVATE "${fixture}")
        target_include_directories(${unit} SYSTEM PRIVATE "${device}" "${cmsis}")
        target_compile_options(${unit} PRIVATE
            "-include${source}/tests/google/stm32_official_model.h"
            -ffunction-sections -fdata-sections)
    endforeach()
    # These are upstream 32-bit register addresses in an unchanged vendor file.
    target_compile_options(${target}_vendor PRIVATE -Wno-int-to-pointer-cast
        -Wno-pointer-to-int-cast)
    if(TARGET nexus_build_options)
        target_link_libraries(${target}_vendor PRIVATE nexus_build_options)
    endif()
    target_link_libraries(${target} PRIVATE ${target}_vendor)
    target_link_options(${target} PRIVATE -Wl,--gc-sections)
endfunction()

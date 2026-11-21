# Exact selected upstream profile; an unselected inventory is never prepared.
# Author: Nexus Team
include_guard(GLOBAL)

function(nexus_add_cmsis_dsp)
    if(TARGET Nexus::CMSISDSP)
        return()
    endif()
    get_filename_component(
        cmsis_owner_root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../.."
        ABSOLUTE
    )
    include("${cmsis_owner_root}/cmake/dependencies/ComponentSource.cmake")
    get_property(
        cmsis_dsp_family
        TARGET Nexus::Config
        PROPERTY NEXUS_SOC_FAMILY
    )
    if(NOT cmsis_dsp_family)
        message(
            FATAL_ERROR
            "CMSIS-DSP requires the resolved Nexus family property"
        )
    endif()
    nexus_component_source(cmsis_dsp cmsis_dsp_source)
    set(cmsis_dsp_sources
        Source/BasicMathFunctions/arm_dot_prod_f32.c
        Source/FilteringFunctions/arm_fir_init_f32.c
        Source/FilteringFunctions/arm_fir_f32.c
        Source/FilteringFunctions/arm_biquad_cascade_df1_init_f32.c
        Source/FilteringFunctions/arm_biquad_cascade_df1_f32.c
        Source/TransformFunctions/arm_rfft_fast_init_f32.c
        Source/TransformFunctions/arm_rfft_fast_f32.c
        Source/TransformFunctions/arm_cfft_init_f32.c
        Source/TransformFunctions/arm_cfft_f32.c
        Source/TransformFunctions/arm_cfft_radix8_f32.c
        Source/TransformFunctions/arm_bitreversal2.c
        Source/CommonTables/arm_common_tables.c
        Source/CommonTables/arm_const_structs.c
    )
    list(TRANSFORM cmsis_dsp_sources PREPEND "${cmsis_dsp_source}/")
    add_library(nexus_cmsis_dsp STATIC EXCLUDE_FROM_ALL ${cmsis_dsp_sources})
    nexus_apply_target_policy(nexus_cmsis_dsp)
    target_include_directories(
        nexus_cmsis_dsp
        SYSTEM
        PRIVATE
            "${cmsis_dsp_source}/Include"
            "${cmsis_dsp_source}/PrivateInclude"
    )
    target_link_libraries(nexus_cmsis_dsp PRIVATE nexus_build_options)
    target_compile_definitions(nexus_cmsis_dsp PRIVATE DISABLEFLOAT16)
    if(cmsis_dsp_family STREQUAL "native")
        target_compile_definitions(nexus_cmsis_dsp PRIVATE __GNUC_PYTHON__)
    else()
        nexus_use_cmsis_core(nexus_cmsis_dsp)
    endif()
    add_library(Nexus::CMSISDSP ALIAS nexus_cmsis_dsp)

    add_library(
        nexus_algorithms_cmsis_dsp
        STATIC
        EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/cmsis_dsp.c"
    )
    nexus_apply_target_policy(nexus_algorithms_cmsis_dsp)
    target_include_directories(
        nexus_algorithms_cmsis_dsp
        PUBLIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include"
    )
    target_include_directories(
        nexus_algorithms_cmsis_dsp
        SYSTEM
        PRIVATE
            "${cmsis_dsp_source}/Include"
            "${cmsis_dsp_source}/PrivateInclude"
    )
    target_link_libraries(
        nexus_algorithms_cmsis_dsp
        PUBLIC Nexus::Core
        PRIVATE Nexus::CMSISDSP nexus_build_options nexus_owned_warnings
    )
    target_compile_definitions(
        nexus_algorithms_cmsis_dsp
        PRIVATE DISABLEFLOAT16
    )
    if(cmsis_dsp_family STREQUAL "native")
        target_compile_definitions(
            nexus_algorithms_cmsis_dsp
            PRIVATE __GNUC_PYTHON__
        )
    else()
        nexus_use_cmsis_core(nexus_algorithms_cmsis_dsp)
    endif()
    add_library(Nexus::AlgorithmsCMSISDSP ALIAS nexus_algorithms_cmsis_dsp)
endfunction()

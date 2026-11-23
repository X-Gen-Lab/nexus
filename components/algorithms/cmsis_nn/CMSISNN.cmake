# Selected signed-byte inference has one source and private vendor boundary.
# Author: Nexus Team
include_guard(GLOBAL)

function(nexus_add_cmsis_nn)
    if(TARGET Nexus::AlgorithmsCMSISNN)
        return()
    endif()
    get_filename_component(
        nn_root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../.."
        ABSOLUTE
    )
    include("${nn_root}/cmake/dependencies/ComponentSource.cmake")
    get_property(nn_family TARGET Nexus::Config PROPERTY NEXUS_SOC_FAMILY)
    if(NOT nn_family MATCHES "^(native|stm32f4)$")
        message(FATAL_ERROR "CMSIS-NN profile requires Native or STM32F4")
    endif()
    nexus_component_source(cmsis_nn nn_source)
    add_library(
        nexus_cmsis_nn
        STATIC
        EXCLUDE_FROM_ALL
        "${nn_source}/Source/FullyConnectedFunctions/arm_fully_connected_s8.c"
        "${nn_source}/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s8.c"
    )
    nexus_apply_target_policy(nexus_cmsis_nn)
    target_include_directories(
        nexus_cmsis_nn
        SYSTEM
        PRIVATE "${nn_source}/Include"
    )
    target_link_libraries(nexus_cmsis_nn PRIVATE nexus_build_options)
    add_library(Nexus::CMSISNN ALIAS nexus_cmsis_nn)
    add_library(
        nexus_algorithms_cmsis_nn
        STATIC
        EXCLUDE_FROM_ALL
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/cmsis_nn.c"
    )
    nexus_apply_target_policy(nexus_algorithms_cmsis_nn)
    target_include_directories(
        nexus_algorithms_cmsis_nn
        PUBLIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include"
    )
    target_include_directories(
        nexus_algorithms_cmsis_nn
        SYSTEM
        PRIVATE "${nn_source}/Include"
    )
    target_link_libraries(
        nexus_algorithms_cmsis_nn
        PUBLIC Nexus::Core
        PRIVATE Nexus::CMSISNN nexus_build_options nexus_owned_warnings
    )
    if(nn_family STREQUAL "stm32f4")
        nexus_use_cmsis_core(nexus_cmsis_nn)
        nexus_use_cmsis_core(nexus_algorithms_cmsis_nn)
    endif()
    add_library(Nexus::AlgorithmsCMSISNN ALIAS nexus_algorithms_cmsis_nn)
endfunction()

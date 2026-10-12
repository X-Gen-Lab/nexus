# Selected EXTI ownership; only CPU and physical register effects are modeled.
nexus_google_test(nexus_gd32_exti_test SOURCES gd32_exti_test.cpp
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/drivers/exti.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470/private")
target_include_directories(nexus_gd32_exti_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_exti_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_exti_test PRIVATE -Wno-int-to-pointer-cast
    "-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_exti_model.h")

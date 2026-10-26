# Root includes this independent selected test registration after source freeze.
# Common io/src/api/adc_group.c is added to Nexus::IO
# only after actual missing implementation RED. Initial RED omits missing C
# source rather than failing CMake on a nonexistent provider path.
nexus_google_test(nexus_g474_adc_group_test
    SOURCES g474_adc_group_test.cpp g474_adc_group_model.cpp
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/soc/common"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/private")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_g474_adc_group_test g4)
target_include_directories(nexus_g474_adc_group_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_adc_group_test)
target_compile_definitions(nexus_g474_adc_group_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_adc_group_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_adc_group_model.h")
# Parent recorded genuine compile0 / missing-symbol link RED before creation.
target_sources(nexus_g474_adc_group_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc_group.c")
# Common production source io/src/api/adc_group.c belongs to Nexus::IO's root graph.

nexus_google_test(nexus_adc_group_test
    SOURCES adc_group_test.cpp LIBRARIES Nexus::IO)

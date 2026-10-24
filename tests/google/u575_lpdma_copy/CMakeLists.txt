# Target-local model boundary; never affects another production compilation.
# Author: Nexus Team
nexus_google_test(nexus_u575_lpdma_copy_test
    SOURCES test.cpp model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/lpdma_copy.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_lpdma_copy/include"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_lpdma_copy/private"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32u575_retained_time/include"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private")
target_compile_definitions(nexus_u575_lpdma_copy_test PRIVATE STM32U575xx
    NX_U575_LPDMA_COPY_MODEL)
target_include_directories(nexus_u575_lpdma_copy_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include"
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32u5_hal/Inc")

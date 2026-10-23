# Real selected pre-RAM C; only constant MMIO leaves are modeled.
nexus_google_test(nexus_u575_memory_test
    SOURCES u575_memory_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/memory_init.c"
    LIBRARIES Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_memory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_memory_test PRIVATE STM32U575xx)
target_compile_options(nexus_u575_memory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_memory_model.h")

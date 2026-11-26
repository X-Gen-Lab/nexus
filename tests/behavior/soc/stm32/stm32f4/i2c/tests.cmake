# Explicit i2c behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/soc/stm32/stm32f4/i2c/stm32_i2c_cold.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_target.cmake")

include(
    "${nexus_google_root}/soc/stm32/stm32f4/i2c/stm32_i2c_poll_factory.cmake"
)

nexus_google_test(nexus_stm32_i2c_register_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_register_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c/i2c_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio/gpio.c"
        "${nexus_google_root}/soc/stm32/stm32f4/i2c/stm32_i2c_storage_model.c"
    LIBRARIES Nexus::IO
    INCLUDES
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async" "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/arch/include"
)

target_include_directories(
    nexus_stm32_i2c_register_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_stm32_i2c_register_test)

target_compile_definitions(
    nexus_stm32_i2c_register_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx NEXUS_I2C_STM32_TEST
)

target_compile_options(
    nexus_stm32_i2c_register_test
    PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-include${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_register_model.h>"
)

foreach(i2c_family generic stm32)
    set(i2c_target "nexus_${i2c_family}_i2c_async_test")
    nexus_google_test(${i2c_target} SOURCES "${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_async_register_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async"
    )
    target_compile_options(
        ${i2c_target}
        PRIVATE
            "-include${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_async_register_model.h"
    )
    if(i2c_family STREQUAL "stm32")
        target_sources(
            ${i2c_target}
            PRIVATE
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c/i2c_async.c"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c/i2c_cold.c"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio/gpio.c"
                "${nexus_google_root}/soc/stm32/stm32f4/i2c/stm32_i2c_storage_model.c"
        )
        include("${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sdk/STM32Official.cmake")
        nexus_stm32f4_official(${i2c_target})
        target_include_directories(
            ${i2c_target}
            PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        )
        target_include_directories(
            ${i2c_target}
            SYSTEM
            PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
                "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
        )
        nexus_use_cmsis_core(${i2c_target})
        target_compile_definitions(
            ${i2c_target}
            PRIVATE NX_I2C_TEST_STM32 NEXUS_STM32_MODEL STM32F407xx
        )
    endif()
endforeach()

include("${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_pair_factory.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/i2c/i2c_multi_controller.cmake")

# Apply the reviewed LL port after all I2C consumers have been declared.
include("${nexus_google_root}/soc/stm32/stm32f4/i2c/official_leaf_probes.cmake")

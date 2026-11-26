# Explicit entropy behavior and private model dependencies.
# Author: Nexus Team
# The private entropy health tests use real reviewed LL over a register model.
nexus_google_test(nexus_entropy_test STM32F4_LL SOURCES "${nexus_google_root}/soc/stm32/stm32f4/entropy/entropy_test.cpp"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/entropy/entropy.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
)

target_include_directories(
    nexus_entropy_test
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
)

nexus_use_cmsis_core(nexus_entropy_test)

target_compile_definitions(
    nexus_entropy_test
    PRIVATE STM32F407xx NEXUS_STM32_MODEL
)

target_compile_options(
    nexus_entropy_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/entropy/entropy_model.h"
)

foreach(entropy_profile RANGE 0 3)
    set(entropy_target "nexus_entropy_profile_${entropy_profile}_test")
    nexus_google_test(${entropy_target} STM32F4_LL
        SOURCES "${nexus_google_root}/soc/stm32/stm32f4/entropy/entropy_profile_test.cpp"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/entropy/entropy.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
    )
    target_include_directories(
        ${entropy_target}
        SYSTEM
        PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    )
    nexus_use_cmsis_core(${entropy_target})
    target_compile_definitions(
        ${entropy_target}
        PRIVATE
            STM32F407xx
            NEXUS_STM32_MODEL
            NX_ENTROPY_PROFILE=${entropy_profile}
    )
    target_compile_options(
        ${entropy_target}
        PRIVATE
            "-include${nexus_google_root}/soc/stm32/stm32f4/entropy/entropy_profile_model.h"
    )
endforeach()

include("${nexus_google_root}/soc/stm32/stm32f4/entropy/entropy_factory.cmake")

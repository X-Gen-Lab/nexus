# Exact selected authority uses real task, public GPIO and U575 provider C.
nexus_google_test(nexus_freertos_mpu_gpio_test
    SOURCES freertos_mpu_gpio_test.cpp u575_mpu_user_init_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu/gpio.c"
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu/task.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/mpu_user_init.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/google/mpu_profile"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/os/freertos/include"
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/private")
target_include_directories(nexus_freertos_mpu_gpio_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/ext/freertos/include"
    "${NEXUS_SOURCE_DIR}/ext/freertos/portable/GCC/ARM_CM33_NTZ/non_secure"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_freertos_mpu_gpio_test PRIVATE STM32U575xx
    NEXUS_OS_MPU_GPIO=1 NEXUS_ARCH_MPU_VERSION=8 NEXUS_CPU_HAS_BASEPRI=1
    NX_U575_MPU_USER_MODEL=1)
target_compile_options(nexus_freertos_mpu_gpio_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/freertos_mpu_gpio_model.h")

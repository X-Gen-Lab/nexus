# Production restricted-task code with real pinned port types and mocked kernel.
function(nexus_freertos_mpu_model name version port)
    nexus_google_test(${name} SOURCES freertos_mpu_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu/task.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/google/mpu_profile"
            "${NEXUS_SOURCE_DIR}/os/freertos/include"
            "${NEXUS_SOURCE_DIR}/os/freertos/mpu"
            "${NEXUS_SOURCE_DIR}/core/include"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/${port}")
    target_compile_definitions(${name} PRIVATE
        NEXUS_ARCH_MPU_VERSION=${version} NEXUS_CPU_HAS_BASEPRI=1)
endfunction()
nexus_freertos_mpu_model(nexus_freertos_mpu_v7_test 7 GCC/ARM_CM4_MPU)
nexus_freertos_mpu_model(nexus_freertos_mpu_v8_test 8 GCC/ARM_CM33_NTZ/non_secure)

# Cold bootstrap admission uses the production guard. Register snapshots are
# mocked; each world's EXC_RETURN is selected by the real kernel config macro.
function(nexus_freertos_mpu_guard_model name version port secure)
    nexus_google_test(${name} SOURCES freertos_mpu_guard_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu/guard.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/google/mpu_profile"
            "${NEXUS_SOURCE_DIR}/os/freertos/mpu")
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/GCC/${port}")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_MPU_VERSION=${version}
        NEXUS_CPU_HAS_BASEPRI=1 configRUN_FREERTOS_SECURE_ONLY=${secure})
endfunction()
nexus_freertos_mpu_guard_model(nexus_freertos_mpu_guard_v7_test 7 ARM_CM4_MPU 0)
nexus_freertos_mpu_guard_model(nexus_freertos_mpu_guard_v8_ns_test 8
    ARM_CM33_NTZ/non_secure 0)
nexus_freertos_mpu_guard_model(nexus_freertos_mpu_guard_v8_single_test 8
    ARM_CM33_NTZ/non_secure 1)

# Extract exact hash-checked derived fallback/MPU-setup function bodies. Only
# opcode fetch, ISA barriers and hardware/linker boundaries become mocks.
function(nexus_freertos_mpu_port_model name version port secure)
    set(source "${NEXUS_SOURCE_DIR}/ext/freertos/portable/GCC/${port}/port.c")
    set(model "${CMAKE_CURRENT_BINARY_DIR}/${name}/port-model.c")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}"
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu/prepare_port.py"
        "${NEXUS_SOURCE_DIR}/tests/google/mpu_port_model.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tests/google/mpu_port_model.py"
        --source "${source}" --output "${model}"
        RESULT_VARIABLE model_result ERROR_VARIABLE model_error)
    if(NOT model_result EQUAL 0)
        message(FATAL_ERROR "Actual MPU port model rejected: ${model_error}")
    endif()
    nexus_google_test(${name} SOURCES freertos_mpu_port_test.cpp "${model}"
        "${NEXUS_SOURCE_DIR}/os/freertos/mpu/guard.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/google/mpu_profile"
            "${NEXUS_SOURCE_DIR}/tests/google"
            "${NEXUS_SOURCE_DIR}/os/freertos/mpu")
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/ext/freertos/include"
        "${NEXUS_SOURCE_DIR}/ext/freertos/portable/GCC/${port}")
    target_compile_definitions(${name} PRIVATE NEXUS_ARCH_MPU_VERSION=${version}
        NEXUS_CPU_HAS_BASEPRI=1 configRUN_FREERTOS_SECURE_ONLY=${secure})
endfunction()
nexus_freertos_mpu_port_model(nexus_freertos_mpu_port_m3_test 7 ARM_CM3_MPU 0)
nexus_freertos_mpu_port_model(nexus_freertos_mpu_port_m4_test 7 ARM_CM4_MPU 0)
nexus_freertos_mpu_port_model(nexus_freertos_mpu_port_v8_ns_test 8
    ARM_CM33_NTZ/non_secure 0)
nexus_freertos_mpu_port_model(nexus_freertos_mpu_port_v8_single_test 8
    ARM_CM33_NTZ/non_secure 1)

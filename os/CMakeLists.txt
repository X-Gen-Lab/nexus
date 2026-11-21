# CMakeLists.txt
# Optional wait helpers and explicit per-backend storage.
# Author: Nexus Team
add_library(nexus_os_wait STATIC EXCLUDE_FROM_ALL wait.c)
target_include_directories(
    nexus_os_wait
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_os_wait
    PUBLIC nexus_core
    PRIVATE nexus_owned_warnings nexus_build_options
)

add_library(nexus_os_diagnostic STATIC EXCLUDE_FROM_ALL diagnostic.c)
target_include_directories(
    nexus_os_diagnostic
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_os_diagnostic
    PUBLIC nexus_core
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)

add_library(nexus_os_baremetal STATIC EXCLUDE_FROM_ALL baremetal/notify.c)
target_link_libraries(
    nexus_os_baremetal
    PUBLIC nexus_os_wait
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)

if(NEXUS_CPU_ARCH STREQUAL "native" AND NOT _nexus_runtime_scope)
    find_package(Threads REQUIRED)
    if(NEXUS_BACKEND STREQUAL "native")
        include(CheckCSourceCompiles)
        include(CMakePushCheckState)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
        check_c_source_compiles(
            "#define _GNU_SOURCE
             #include <pthread.h>
             #include <time.h>
             int main(void) {
                 struct timespec deadline = {0, 0};
                 pthread_t thread = pthread_self();
                 return pthread_tryjoin_np(thread, 0) +
                     pthread_clockjoin_np(thread, 0, CLOCK_MONOTONIC, &deadline);
             }"
            nexus_native_deadline_join
        )
        cmake_pop_check_state()
        if(NOT nexus_native_deadline_join)
            message(
                FATAL_ERROR
                "Native tasks require GNU monotonic pthread join support"
            )
        endif()
    endif()
    add_library(nexus_os_native STATIC EXCLUDE_FROM_ALL native/native.c)
    target_include_directories(
        nexus_os_native
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/include"
    )
    target_link_libraries(
        nexus_os_native
        PUBLIC nexus_os_wait Threads::Threads
        PRIVATE nexus_owned_warnings nexus_build_options
    )
endif()

if(NEXUS_BACKEND STREQUAL "freertos" AND NOT NEXUS_CPU_ARCH STREQUAL "native")
    add_subdirectory(freertos)
endif()

if(NEXUS_BACKEND STREQUAL "threadx")
    add_subdirectory(threadx)
endif()

# Bind real backend targets, including independent real-kernel test seams.
# Reject mixed identities before archive order can select wrong symbols.
function(_nexus_os_backend target backend)
    set(capabilities
        wait
        tasks
        permanent_tasks
        notify
        queue
        channel
        join_until
        sleep_until
        yield
    )
    if(NOT backend STREQUAL "native")
        list(APPEND capabilities priority)
        # Host kernel ports order tasks but cannot publish a hardware IRQ.
        if(CMAKE_CROSSCOMPILING)
            list(APPEND capabilities isr_notify)
        endif()
    endif()
    set_target_properties(
        ${target}
        PROPERTIES
            NEXUS_OS_CAPABILITIES "${capabilities}"
            NEXUS_OS_BACKEND "${backend}"
            INTERFACE_NEXUS_OS_BACKEND "${backend}"
    )
    set_property(
        TARGET ${target}
        APPEND
        PROPERTY COMPATIBLE_INTERFACE_STRING NEXUS_OS_BACKEND
    )
endfunction()

set_property(TARGET nexus_os_baremetal PROPERTY NEXUS_OS_CAPABILITIES wait)
foreach(backend native freertos threadx)
    if(TARGET nexus_os_${backend})
        _nexus_os_backend(nexus_os_${backend} ${backend})
    endif()
endforeach()

add_library(Nexus::OSWait ALIAS nexus_os_wait)
add_library(Nexus::OSDiagnostic ALIAS nexus_os_diagnostic)
add_library(Nexus::OSBaremetal ALIAS nexus_os_baremetal)
if(TARGET nexus_os_native)
    add_library(Nexus::OSNative ALIAS nexus_os_native)
endif()
if(TARGET nexus_os_freertos)
    add_library(Nexus::OSFreeRTOS ALIAS nexus_os_freertos)
endif()
if(TARGET nexus_os_threadx)
    add_library(Nexus::OSThreadX ALIAS nexus_os_threadx)
endif()

# Consumers use the selected backend without depending on internal target names.
add_library(Nexus::OS ALIAS nexus_os_${NEXUS_BACKEND})

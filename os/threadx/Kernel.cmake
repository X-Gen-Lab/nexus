# Reviewed official source graph; the dependency lock is its sole authority.
include_guard(GLOBAL)

function(nexus_add_threadx_kernel target port tick_hz basepri)
    if(NOT port MATCHES "^(linux|cortex_m4)$" OR
            NOT tick_hz MATCHES "^[1-9][0-9]*$" OR
            NOT basepri MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Invalid reviewed ThreadX kernel selection")
    endif()
    set(source_root "${NEXUS_SOURCE_DIR}/ext/threadx")
    set(lock_path "${NEXUS_SOURCE_DIR}/dependencies/threadx.lock.json")
    set(verifier "${NEXUS_SOURCE_DIR}/tools/maintenance/threadx_lock.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${lock_path}" "${verifier}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${verifier}"
        --source "${source_root}" --lock "${lock_path}" --port "${port}"
        RESULT_VARIABLE verified OUTPUT_VARIABLE verify_output
        ERROR_VARIABLE verify_error)
    if(NOT verified EQUAL 0)
        message(FATAL_ERROR "ThreadX source lock rejected: "
            "${verify_output}${verify_error}")
    endif()
    file(READ "${lock_path}" lock)
    set(sources)
    foreach(section common ports)
        if(section STREQUAL "ports")
            string(JSON count LENGTH "${lock}" ports "${port}")
        else()
            string(JSON count LENGTH "${lock}" common)
        endif()
        if(count LESS 1)
            message(FATAL_ERROR "ThreadX lock contains an empty source graph")
        endif()
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            if(section STREQUAL "ports")
                string(JSON path GET "${lock}" ports "${port}" ${index})
            else()
                string(JSON path GET "${lock}" common ${index})
            endif()
            list(APPEND sources "${source_root}/${path}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${source_root}/${path}")
        endforeach()
    endforeach()
    set(owned_root "${NEXUS_SOURCE_DIR}/os/threadx")
    if(port STREQUAL "linux")
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "Reviewed ThreadX host port requires Linux")
        endif()
        find_package(Threads REQUIRED)
        include(CheckCSourceCompiles)
        set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
        set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
        # A caller cache value never substitutes for the real link probe.
        unset(NEXUS_THREADX_HAS_SEM_CLOCKWAIT CACHE)
        unset(NEXUS_THREADX_HAS_SEM_CLOCKWAIT)
        check_c_source_compiles("#include <semaphore.h>
#include <time.h>
int main(void) { sem_t semaphore; struct timespec deadline = {0, 0};
return sem_clockwait(&semaphore, CLOCK_MONOTONIC, &deadline); }"
            NEXUS_THREADX_HAS_SEM_CLOCKWAIT)
        if(NOT NEXUS_THREADX_HAS_SEM_CLOCKWAIT)
            message(FATAL_ERROR "ThreadX requires linked GNU sem_clockwait")
        endif()
        set(overlay_directory "${CMAKE_CURRENT_BINARY_DIR}/${target}-port")
        file(MAKE_DIRECTORY "${overlay_directory}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${owned_root}/prepare_linux.py")
        foreach(kind lowlevel scheduler)
            if(kind STREQUAL "lowlevel")
                set(filename "tx_initialize_low_level.c")
            else()
                set(filename "tx_thread_schedule.c")
            endif()
            set(original "${source_root}/ports/linux/gnu/src/${filename}")
            set(overlay "${overlay_directory}/${filename}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${original}")
            execute_process(COMMAND "${Python3_EXECUTABLE}" -B
                "${owned_root}/prepare_linux.py" --source "${original}"
                --output "${overlay}" --kind "${kind}"
                RESULT_VARIABLE prepared OUTPUT_VARIABLE prepare_output
                ERROR_VARIABLE prepare_error)
            if(NOT prepared EQUAL 0)
                message(FATAL_ERROR "ThreadX ${kind} overlay rejected: "
                    "${prepare_output}${prepare_error}")
            endif()
            list(REMOVE_ITEM sources "${original}")
            list(APPEND sources "${overlay}")
        endforeach()
    endif()
    add_library(${target} STATIC ${sources})
    target_include_directories(${target} SYSTEM PUBLIC
        "${source_root}/common/inc" "${source_root}/ports/${port}/gnu/inc")
    target_include_directories(${target} PUBLIC "${owned_root}/config")
    target_compile_definitions(${target} PUBLIC TX_INCLUDE_USER_DEFINE_FILE
        "TX_TIMER_TICKS_PER_SECOND=${tick_hz}")
    target_link_libraries(${target} PRIVATE nexus_build_options)
    if(port STREQUAL "linux")
        target_compile_definitions(${target} PUBLIC _GNU_SOURCE)
        target_link_libraries(${target} PUBLIC Threads::Threads)
    elseif(basepri GREATER 0)
        target_compile_definitions(${target} PUBLIC TX_PORT_USE_BASEPRI
            "TX_PORT_BASEPRI=${basepri}")
    endif()
endfunction()

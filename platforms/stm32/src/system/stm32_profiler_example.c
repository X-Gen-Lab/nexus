/**
 * \file            stm32_profiler_example.c
 * \brief           STM32 profiler usage examples
 * \author          Nexus Team
 */

#include "system/stm32_performance.h"
#include "system/stm32_profiler.h"
#include <inttypes.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Example Functions to Profile                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Fast function example
 */
static void fast_function(void) {
    STM32_PROFILE_FUNCTION("fast_function");

    volatile uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
    }
}

/**
 * \brief           Medium function example
 */
static void medium_function(void) {
    STM32_PROFILE_FUNCTION("medium_function");

    volatile uint32_t sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i * i;
    }
}

/**
 * \brief           Slow function example
 */
static void slow_function(void) {
    STM32_PROFILE_FUNCTION("slow_function");

    volatile uint32_t sum = 0;
    for (int i = 0; i < 10000; i++) {
        sum += i * i * i;
    }
}

/**
 * \brief           Function with nested profiling
 */
static void nested_function(void) {
    STM32_PROFILE_FUNCTION("nested_function");

    /* Profile a specific code block */
    STM32_PROFILE_ZONE("nested_function::loop1") {
        volatile uint32_t sum = 0;
        for (int i = 0; i < 500; i++) {
            sum += i;
        }
    }

    /* Profile another code block */
    STM32_PROFILE_ZONE("nested_function::loop2") {
        volatile uint32_t sum = 0;
        for (int i = 0; i < 500; i++) {
            sum += i * 2;
        }
    }
}

/**
 * \brief           Simulate data processing
 */
static void process_data(uint8_t* data, size_t size) {
    STM32_PROFILE_FUNCTION("process_data");

    STM32_PROFILE_ZONE("process_data::validation") {
        /* Validate data */
        if (data == NULL || size == 0) {
            return;
        }
    }

    STM32_PROFILE_ZONE("process_data::processing") {
        /* Process data */
        volatile uint32_t checksum = 0;
        for (size_t i = 0; i < size; i++) {
            checksum += data[i];
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Profiler Examples                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 1: Basic profiling
 */
void profiler_example_basic(void) {
    printf("\n=== Example 1: Basic Profiling ===\n");

    /* Initialize profiler */
    stm32_profiler_init();

    /* Call functions multiple times */
    for (int i = 0; i < 10; i++) {
        fast_function();
        medium_function();
        slow_function();
    }

    /* Print report */
    stm32_profiler_report();
}

/**
 * \brief           Example 2: Nested profiling
 */
void profiler_example_nested(void) {
    printf("\n=== Example 2: Nested Profiling ===\n");

    /* Reset profiler */
    stm32_profiler_reset();

    /* Call nested function */
    for (int i = 0; i < 5; i++) {
        nested_function();
    }

    /* Print report */
    stm32_profiler_report();
}

/**
 * \brief           Example 3: Hot path analysis
 */
void profiler_example_hot_paths(void) {
    printf("\n=== Example 3: Hot Path Analysis ===\n");

    /* Reset profiler */
    stm32_profiler_reset();

    /* Simulate workload with different call frequencies */
    for (int i = 0; i < 100; i++) {
        fast_function();
    }

    for (int i = 0; i < 50; i++) {
        medium_function();
    }

    for (int i = 0; i < 10; i++) {
        slow_function();
    }

    /* Print hot path report */
    stm32_profiler_hot_path_report(5);
}

/**
 * \brief           Example 4: Real-world scenario
 */
void profiler_example_real_world(void) {
    printf("\n=== Example 4: Real-World Scenario ===\n");

    /* Reset profiler */
    stm32_profiler_reset();

    /* Simulate real-world workload */
    uint8_t buffer[256];

    for (int i = 0; i < 20; i++) {
        /* Initialize buffer */
        STM32_PROFILE_ZONE("init_buffer") {
            for (int j = 0; j < 256; j++) {
                buffer[j] = j;
            }
        }

        /* Process data */
        process_data(buffer, 256);

        /* Transmit data (simulated) */
        STM32_PROFILE_ZONE("transmit_data") {
            volatile uint32_t sum = 0;
            for (int j = 0; j < 256; j++) {
                sum += buffer[j];
            }
        }
    }

    /* Print sorted report */
    stm32_profiler_report_sorted();
}

/**
 * \brief           Example 5: Performance comparison
 */
void profiler_example_comparison(void) {
    printf("\n=== Example 5: Performance Comparison ===\n");

    /* Reset profiler */
    stm32_profiler_reset();

    /* Compare different implementations */

    /* Implementation 1: Naive loop */
    STM32_PROFILE_ZONE("impl1_naive") {
        volatile uint32_t sum = 0;
        for (int i = 0; i < 1000; i++) {
            sum += i;
        }
    }

    /* Implementation 2: Unrolled loop */
    STM32_PROFILE_ZONE("impl2_unrolled") {
        volatile uint32_t sum = 0;
        for (int i = 0; i < 1000; i += 4) {
            sum += i;
            sum += i + 1;
            sum += i + 2;
            sum += i + 3;
        }
    }

    /* Implementation 3: Formula-based */
    STM32_PROFILE_ZONE("impl3_formula") {
        volatile uint32_t sum = (1000 * (1000 - 1)) / 2;
        (void)sum; /* Prevent unused variable warning */
    }

    /* Print report */
    stm32_profiler_report();

    printf("\nConclusion:\n");
    printf("  - Formula-based approach is fastest\n");
    printf("  - Unrolled loop is faster than naive loop\n");
    printf("  - Choose implementation based on requirements\n");
}

/**
 * \brief           Run all profiler examples
 */
void stm32_profiler_run_examples(void) {
    printf("\n");
    printf("========================================\n");
    printf("STM32 Profiler Examples\n");
    printf("========================================\n");

    /* Run examples */
    profiler_example_basic();
    profiler_example_nested();
    profiler_example_hot_paths();
    profiler_example_real_world();
    profiler_example_comparison();

    printf("\n========================================\n");
    printf("Profiler Examples Complete\n");
    printf("========================================\n\n");
}

/**
 * \brief           Demonstrate profiler statistics
 */
void stm32_profiler_demo_stats(void) {
    stm32_profiler_stats_t stats;

    if (stm32_profiler_get_stats(&stats) == 0) {
        printf("\n=== Profiler Statistics ===\n");
        printf("Total zones:  %" PRIu32 "\n", stats.total_zones);
        printf("Active zones: %" PRIu32 "\n", stats.active_zones);
        printf("Total samples: %" PRIu32 "\n", stats.total_samples);

        if (stats.overflow_count > 0) {
            printf("Overflows: %" PRIu32
                   " (increase STM32_PROFILER_MAX_ZONES)\n",
                   stats.overflow_count);
        }

        printf("===========================\n\n");
    }
}

/**
 * \file            stm32_performance_test.c
 * \brief           STM32 performance measurement test and demonstration
 * \author          Nexus Team
 */

#include "boot/stm32_boot.h"
#include "system/stm32_performance.h"
#include <inttypes.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* Test Functions                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test function for profiling
 */
static void test_function_fast(void) {
    volatile uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
    }
}

/**
 * \brief           Test function for profiling (slower)
 */
static void test_function_slow(void) {
    volatile uint32_t sum = 0;
    for (int i = 0; i < 10000; i++) {
        sum += i;
    }
}

/*---------------------------------------------------------------------------*/
/* Performance Test Main                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Run performance measurement tests
 * \details         Demonstrates boot time, code size, and profiling features
 */
void stm32_performance_test_run(void) {
    printf("\n");
    printf("========================================\n");
    printf("STM32 Performance Measurement Test\n");
    printf("========================================\n");

    /* Test 1: Boot Time Measurement */
    printf("\n[Test 1] Boot Time Measurement\n");
    stm32_boot_time_report();

    /* Test 2: Code Size Measurement */
    printf("\n[Test 2] Code Size Measurement\n");
    stm32_code_size_report();

    /* Test 3: Function Profiling */
    printf("\n[Test 3] Function Profiling\n");
    printf("Profiling test functions...\n");
    stm32_perf_profile_function(test_function_fast, "test_function_fast");
    stm32_perf_profile_function(test_function_slow, "test_function_slow");

    /* Test 4: Manual Performance Counter */
    printf("\n[Test 4] Manual Performance Counter\n");
    stm32_perf_counter_t counter;

    stm32_perf_start(&counter);
    /* Simulate some work */
    for (volatile int i = 0; i < 1000; i++)
        ;
    stm32_perf_stop(&counter);

    printf("Manual counter test: %" PRIu32 " cycles (%" PRIu32 " us)\n",
           counter.elapsed_cycles, counter.elapsed_us);

    /* Test 5: Platform Status */
    printf("\n[Test 5] Platform Status\n");
    printf("Platform initialized: %s\n",
           stm32_platform_is_initialized() ? "Yes" : "No");
    printf("System clock: %" PRIu32 " Hz (%.2f MHz)\n",
           stm32_platform_get_sysclk(),
           stm32_platform_get_sysclk() / 1000000.0f);

    printf("\n========================================\n");
    printf("Performance Test Complete\n");
    printf("========================================\n\n");
}

/**
 * \brief           Run boot time optimization analysis
 * \details         Provides recommendations for boot time optimization
 */
void stm32_boot_time_analyze(void) {
    stm32_boot_time_t boot_time;

    if (stm32_boot_time_get(&boot_time) != 0) {
        printf("Boot time not measured\n");
        return;
    }

    printf("\n");
    printf("=== Boot Time Optimization Analysis ===\n");

    /* Analyze each stage */
    uint32_t reset_to_systeminit_us =
        stm32_perf_cycles_to_us(boot_time.reset_to_systeminit);
    uint32_t systeminit_to_main_us =
        stm32_perf_cycles_to_us(boot_time.systeminit_to_main);
    uint32_t main_to_platform_init_us =
        stm32_perf_cycles_to_us(boot_time.main_to_platform_init);

    printf("\nStage 1: Reset to SystemInit (%" PRIu32 " us)\n",
           reset_to_systeminit_us);
    if (reset_to_systeminit_us > 1000) {
        printf("  [!] Optimization: Check startup code efficiency\n");
        printf("      - Minimize .data section size\n");
        printf("      - Reduce .bss section size\n");
    } else {
        printf("  [OK] Within expected range\n");
    }

    printf("\nStage 2: SystemInit to main() (%" PRIu32 " us)\n",
           systeminit_to_main_us);
    if (systeminit_to_main_us > 1000) {
        printf("  [!] Optimization: Check C runtime initialization\n");
        printf("      - Minimize global constructors\n");
        printf("      - Reduce static initialization\n");
    } else {
        printf("  [OK] Within expected range\n");
    }

    printf("\nStage 3: main() to platform_init (%" PRIu32 " us)\n",
           main_to_platform_init_us);
    if (main_to_platform_init_us > 5000) {
        printf("  [!] Optimization: Check platform initialization\n");
        printf("      - Optimize clock configuration\n");
        printf("      - Defer non-critical initialization\n");
        printf("      - Use HSI instead of HSE if acceptable\n");
    } else {
        printf("  [OK] Within expected range\n");
    }

    printf("\nTotal: %" PRIu32 " us (%.2f ms)\n", boot_time.total_boot_time_us,
           boot_time.total_boot_time_us / 1000.0f);

    if (boot_time.total_boot_time_us < 10000) {
        printf("Status: EXCELLENT - Meets < 10ms requirement\n");
    } else if (boot_time.total_boot_time_us < 15000) {
        printf("Status: GOOD - Close to 10ms target\n");
    } else {
        printf("Status: NEEDS OPTIMIZATION - Exceeds 10ms target\n");
    }

    printf("========================================\n\n");
}

/**
 * \brief           Run code size optimization analysis
 * \details         Provides recommendations for code size optimization
 */
void stm32_code_size_analyze(void) {
    stm32_code_size_t code_size;

    if (stm32_code_size_get(&code_size) != 0) {
        printf("Code size measurement failed\n");
        return;
    }

    printf("\n");
    printf("=== Code Size Optimization Analysis ===\n");

    printf("\nFlash Usage: %" PRIu32 " bytes (%.2f KB)\n",
           code_size.total_flash, code_size.total_flash / 1024.0f);

    if (code_size.total_flash > 10240) {
        printf("  [!] Optimization recommendations:\n");
        printf("      - Enable LTO (Link Time Optimization)\n");
        printf("      - Use -Os optimization level\n");
        printf("      - Enable -ffunction-sections -fdata-sections\n");
        printf("      - Use --gc-sections linker flag\n");
        printf("      - Disable unused HAL modules\n");
        printf("      - Use LL (Low-Layer) drivers instead of HAL\n");
    } else {
        printf("  [OK] Meets < 10KB requirement for core functionality\n");
    }

    printf("\nRAM Usage: %" PRIu32 " bytes (%.2f KB)\n", code_size.total_ram,
           code_size.total_ram / 1024.0f);

    if (code_size.data_size > 1024) {
        printf("  [!] .data section large (%" PRIu32 " bytes)\n",
               code_size.data_size);
        printf("      - Reduce initialized global variables\n");
        printf("      - Use const for read-only data\n");
    }

    if (code_size.bss_size > 4096) {
        printf("  [!] .bss section large (%" PRIu32 " bytes)\n",
               code_size.bss_size);
        printf("      - Reduce uninitialized global variables\n");
        printf("      - Use stack or heap allocation instead\n");
    }

    printf("========================================\n\n");
}

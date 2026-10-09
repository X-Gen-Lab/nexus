/**
 * \file            stm32_performance.c
 * \brief           STM32 performance measurement and optimization
 * implementation
 * \author          Nexus Team
 */

#include "system/stm32_performance.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* External Symbols from Linker Script                                      */
/*---------------------------------------------------------------------------*/

extern uint32_t _stext; /* Start of .text section */
extern uint32_t _etext; /* End of .text section */
extern uint32_t _sdata; /* Start of .data section */
extern uint32_t _edata; /* End of .data section */
extern uint32_t _sbss;  /* Start of .bss section */
extern uint32_t _ebss;  /* End of .bss section */

/*---------------------------------------------------------------------------*/
/* DWT (Data Watchpoint and Trace) Registers                                */
/*---------------------------------------------------------------------------*/

#define DWT_CTRL (*(volatile uint32_t*)0xE0001000) /* DWT Control Register */
#define DWT_CYCCNT                                                             \
    (*(volatile uint32_t*)0xE0001004) /* DWT Cycle Count Register */
#define DEM_CR                                                                 \
    (*(volatile uint32_t*)0xE000EDFC) /* Debug Exception and Monitor Control   \
                                         Register */
#define DEM_CR_TRCENA      (1 << 24)  /* Trace enable bit */
#define DWT_CTRL_CYCCNTENA (1 << 0)   /* Cycle counter enable bit */

/*---------------------------------------------------------------------------*/
/* Boot Time Measurement State                                              */
/*---------------------------------------------------------------------------*/

static uint32_t g_boot_checkpoints[4] = {0};
static bool g_boot_time_measured = false;

/*---------------------------------------------------------------------------*/
/* Performance Measurement Functions                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DWT for cycle counting
 * \details         Enables DWT and cycle counter for performance measurement
 */
int stm32_perf_init(void) {
    /* Enable trace */
    DEM_CR |= DEM_CR_TRCENA;

    /* Reset cycle counter */
    DWT_CYCCNT = 0;

    /* Enable cycle counter */
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;

    /* Verify cycle counter is running */
    uint32_t start = DWT_CYCCNT;
    for (volatile int i = 0; i < 100; i++)
        ;
    uint32_t end = DWT_CYCCNT;

    if (end <= start) {
        return -1; /* Cycle counter not working */
    }

    return 0;
}

/**
 * \brief           Start performance counter
 */
void stm32_perf_start(stm32_perf_counter_t* counter) {
    if (counter == NULL) {
        return;
    }

    counter->start_cycles = DWT_CYCCNT;
    counter->end_cycles = 0;
    counter->elapsed_cycles = 0;
    counter->elapsed_us = 0;
}

/**
 * \brief           Stop performance counter and calculate elapsed time
 */
void stm32_perf_stop(stm32_perf_counter_t* counter) {
    if (counter == NULL) {
        return;
    }

    counter->end_cycles = DWT_CYCCNT;

    /* Handle counter overflow (32-bit wrap-around) */
    if (counter->end_cycles >= counter->start_cycles) {
        counter->elapsed_cycles = counter->end_cycles - counter->start_cycles;
    } else {
        counter->elapsed_cycles =
            (0xFFFFFFFF - counter->start_cycles) + counter->end_cycles + 1;
    }

    counter->elapsed_us = stm32_perf_cycles_to_us(counter->elapsed_cycles);
}

/**
 * \brief           Get current cycle count
 */
uint32_t stm32_perf_get_cycles(void) {
    return DWT_CYCCNT;
}

/**
 * \brief           Convert cycles to microseconds
 */
uint32_t stm32_perf_cycles_to_us(uint32_t cycles) {
    extern uint32_t SystemCoreClock;

    /* Avoid division by zero */
    if (SystemCoreClock == 0) {
        return 0;
    }

    /* Convert: cycles / (cycles_per_second / 1000000) = microseconds */
    uint64_t us = ((uint64_t)cycles * 1000000ULL) / SystemCoreClock;

    return (uint32_t)us;
}

/*---------------------------------------------------------------------------*/
/* Boot Time Measurement                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mark boot time checkpoint
 * \details         Records cycle count at specific boot stages
 * \note            Checkpoint 0: Reset_Handler entry
 *                  Checkpoint 1: SystemInit complete
 *                  Checkpoint 2: main() entry
 *                  Checkpoint 3: platform_init complete
 */
void stm32_boot_time_mark(uint8_t checkpoint) {
    if (checkpoint >= 4) {
        return;
    }

    g_boot_checkpoints[checkpoint] = DWT_CYCCNT;

    /* Mark as measured when all checkpoints are recorded */
    if (checkpoint == 3) {
        g_boot_time_measured = true;
    }
}

/**
 * \brief           Get boot time measurements
 */
int stm32_boot_time_get(stm32_boot_time_t* boot_time) {
    if (boot_time == NULL || !g_boot_time_measured) {
        return -1;
    }

    /* Calculate time between checkpoints */
    boot_time->reset_to_systeminit =
        g_boot_checkpoints[1] - g_boot_checkpoints[0];
    boot_time->systeminit_to_main =
        g_boot_checkpoints[2] - g_boot_checkpoints[1];
    boot_time->main_to_platform_init =
        g_boot_checkpoints[3] - g_boot_checkpoints[2];

    /* Platform init time is included in main_to_platform_init */
    boot_time->platform_init_time = boot_time->main_to_platform_init;

    /* Total boot time from reset to platform_init complete */
    uint32_t total_cycles = g_boot_checkpoints[3] - g_boot_checkpoints[0];
    boot_time->total_boot_time_us = stm32_perf_cycles_to_us(total_cycles);

    return 0;
}

/**
 * \brief           Print boot time report
 * \details         Prints detailed boot time breakdown
 */
void stm32_boot_time_report(void) {
    stm32_boot_time_t boot_time;

    if (stm32_boot_time_get(&boot_time) != 0) {
        printf("Boot time not measured\n");
        return;
    }

    printf("\n");
    printf("=== Boot Time Report ===\n");
    printf("Reset to SystemInit:     %" PRIu32 " us\n",
           stm32_perf_cycles_to_us(boot_time.reset_to_systeminit));
    printf("SystemInit to main():    %" PRIu32 " us\n",
           stm32_perf_cycles_to_us(boot_time.systeminit_to_main));
    printf("main() to platform_init: %" PRIu32 " us\n",
           stm32_perf_cycles_to_us(boot_time.main_to_platform_init));
    printf("------------------------\n");
    printf("Total boot time:         %" PRIu32 " us (%.2f ms)\n",
           boot_time.total_boot_time_us,
           boot_time.total_boot_time_us / 1000.0f);

    /* Check if boot time meets requirement (< 10ms) */
    if (boot_time.total_boot_time_us < 10000) {
        printf("Status: PASS (< 10ms requirement)\n");
    } else {
        printf("Status: FAIL (>= 10ms, optimization needed)\n");
    }
    printf("========================\n\n");
}

/*---------------------------------------------------------------------------*/
/* Code Size Measurement                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get code size measurements
 * \details         Calculates memory usage from linker symbols
 */
int stm32_code_size_get(stm32_code_size_t* code_size) {
    if (code_size == NULL) {
        return -1;
    }

    /* Calculate section sizes from linker symbols */
    code_size->text_size = (uint32_t)&_etext - (uint32_t)&_stext;
    code_size->data_size = (uint32_t)&_edata - (uint32_t)&_sdata;
    code_size->bss_size = (uint32_t)&_ebss - (uint32_t)&_sbss;

    /* Total Flash usage = .text + .data (initialized data stored in Flash) */
    code_size->total_flash = code_size->text_size + code_size->data_size;

    /* Total RAM usage = .data + .bss */
    code_size->total_ram = code_size->data_size + code_size->bss_size;

    return 0;
}

/**
 * \brief           Print code size report
 * \details         Prints detailed memory usage breakdown
 */
void stm32_code_size_report(void) {
    stm32_code_size_t code_size;

    if (stm32_code_size_get(&code_size) != 0) {
        printf("Code size measurement failed\n");
        return;
    }

    printf("\n");
    printf("=== Code Size Report ===\n");
    printf(".text section:  %" PRIu32 " bytes (%.2f KB)\n", code_size.text_size,
           code_size.text_size / 1024.0f);
    printf(".data section:  %" PRIu32 " bytes (%.2f KB)\n", code_size.data_size,
           code_size.data_size / 1024.0f);
    printf(".bss section:   %" PRIu32 " bytes (%.2f KB)\n", code_size.bss_size,
           code_size.bss_size / 1024.0f);
    printf("------------------------\n");
    printf("Total Flash:    %" PRIu32 " bytes (%.2f KB)\n", code_size.total_flash,
           code_size.total_flash / 1024.0f);
    printf("Total RAM:      %" PRIu32 " bytes (%.2f KB)\n", code_size.total_ram,
           code_size.total_ram / 1024.0f);

    /* Check if core functionality meets requirement (< 10KB) */
    if (code_size.total_flash < 10240) {
        printf("Status: PASS (< 10KB requirement)\n");
    } else {
        printf("Status: INFO (>= 10KB, check if only core functionality)\n");
    }
    printf("========================\n\n");
}

/*---------------------------------------------------------------------------*/
/* Performance Profiling                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Profile a function execution time
 * \details         Measures function execution time using DWT cycle counter
 */
uint32_t stm32_perf_profile_function(void (*func)(void), const char* name) {
    if (func == NULL) {
        return 0;
    }

    stm32_perf_counter_t counter;

    /* Start measurement */
    stm32_perf_start(&counter);

    /* Execute function */
    func();

    /* Stop measurement */
    stm32_perf_stop(&counter);

    /* Print result if name provided */
    if (name != NULL) {
        printf("Function '%s': %" PRIu32 " cycles (%" PRIu32 " us)\n", name,
               counter.elapsed_cycles, counter.elapsed_us);
    }

    return counter.elapsed_us;
}

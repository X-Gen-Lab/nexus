/**
 * \file            stm32_performance.h
 * \brief           STM32 performance measurement and optimization
 * \author          Nexus Team
 */

#ifndef STM32_PERFORMANCE_H
#define STM32_PERFORMANCE_H

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Performance Measurement Types                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Performance counter structure
 */
typedef struct {
    uint32_t start_cycles;   /**< Start cycle count */
    uint32_t end_cycles;     /**< End cycle count */
    uint32_t elapsed_cycles; /**< Elapsed cycles */
    uint32_t elapsed_us;     /**< Elapsed time in microseconds */
} stm32_perf_counter_t;

/**
 * \brief           Boot time measurement structure
 */
typedef struct {
    uint32_t reset_to_systeminit;   /**< Reset to SystemInit (cycles) */
    uint32_t systeminit_to_main;    /**< SystemInit to main (cycles) */
    uint32_t main_to_platform_init; /**< main to platform_init (cycles) */
    uint32_t platform_init_time;    /**< platform_init execution (cycles) */
    uint32_t total_boot_time_us;    /**< Total boot time (microseconds) */
} stm32_boot_time_t;

/**
 * \brief           Code size measurement structure
 */
typedef struct {
    uint32_t text_size;   /**< .text section size (bytes) */
    uint32_t data_size;   /**< .data section size (bytes) */
    uint32_t bss_size;    /**< .bss section size (bytes) */
    uint32_t total_flash; /**< Total Flash usage (bytes) */
    uint32_t total_ram;   /**< Total RAM usage (bytes) */
} stm32_code_size_t;

/*---------------------------------------------------------------------------*/
/* Performance Measurement Functions                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize DWT (Data Watchpoint and Trace) for cycle
 * counting
 * \return          0 on success, -1 on failure
 */
int stm32_perf_init(void);

/**
 * \brief           Start performance counter
 * \param[in]       counter: Pointer to performance counter structure
 */
void stm32_perf_start(stm32_perf_counter_t* counter);

/**
 * \brief           Stop performance counter and calculate elapsed time
 * \param[in]       counter: Pointer to performance counter structure
 */
void stm32_perf_stop(stm32_perf_counter_t* counter);

/**
 * \brief           Get current cycle count
 * \return          Current DWT cycle count
 */
uint32_t stm32_perf_get_cycles(void);

/**
 * \brief           Convert cycles to microseconds
 * \param[in]       cycles: Number of cycles
 * \return          Time in microseconds
 */
uint32_t stm32_perf_cycles_to_us(uint32_t cycles);

/*---------------------------------------------------------------------------*/
/* Boot Time Measurement                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Mark boot time checkpoint
 * \param[in]       checkpoint: Checkpoint identifier (0-3)
 */
void stm32_boot_time_mark(uint8_t checkpoint);

/**
 * \brief           Get boot time measurements
 * \param[out]      boot_time: Pointer to boot time structure
 * \return          0 on success, -1 if not measured
 */
int stm32_boot_time_get(stm32_boot_time_t* boot_time);

/**
 * \brief           Print boot time report
 */
void stm32_boot_time_report(void);

/*---------------------------------------------------------------------------*/
/* Code Size Measurement                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get code size measurements
 * \param[out]      code_size: Pointer to code size structure
 * \return          0 on success, -1 on failure
 */
int stm32_code_size_get(stm32_code_size_t* code_size);

/**
 * \brief           Print code size report
 */
void stm32_code_size_report(void);

/*---------------------------------------------------------------------------*/
/* Performance Profiling                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Profile a function execution time
 * \param[in]       func: Function pointer to profile
 * \param[in]       name: Function name for reporting
 * \return          Execution time in microseconds
 */
uint32_t stm32_perf_profile_function(void (*func)(void), const char* name);

#ifdef __cplusplus
}
#endif

#endif /* STM32_PERFORMANCE_H */

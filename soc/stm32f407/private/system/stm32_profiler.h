/**
 * \file            stm32_profiler.h
 * \brief           STM32 advanced performance profiler
 * \author          Nexus Team
 */

#ifndef STM32_PROFILER_H
#define STM32_PROFILER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Profiler Configuration                                                    */
/*---------------------------------------------------------------------------*/

#ifndef STM32_PROFILER_MAX_ZONES
#define STM32_PROFILER_MAX_ZONES 32 /**< Maximum number of profiling zones */
#endif

#ifndef STM32_PROFILER_MAX_SAMPLES
#define STM32_PROFILER_MAX_SAMPLES 100 /**< Maximum samples per zone */
#endif

/*---------------------------------------------------------------------------*/
/* Profiler Types                                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Profiling zone structure
 */
typedef struct {
    const char* name;      /**< Zone name */
    uint32_t call_count;   /**< Number of calls */
    uint32_t total_cycles; /**< Total cycles */
    uint32_t min_cycles;   /**< Minimum cycles */
    uint32_t max_cycles;   /**< Maximum cycles */
    uint32_t avg_cycles;   /**< Average cycles */
    uint32_t start_cycle;  /**< Start cycle (for active zone) */
    bool active;           /**< Zone is currently active */
} stm32_profile_zone_t;

/**
 * \brief           Profiler statistics
 */
typedef struct {
    uint32_t total_zones;    /**< Total number of zones */
    uint32_t active_zones;   /**< Currently active zones */
    uint32_t total_samples;  /**< Total samples collected */
    uint32_t overflow_count; /**< Number of overflows */
} stm32_profiler_stats_t;

/*---------------------------------------------------------------------------*/
/* Profiler Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize profiler
 * \return          0 on success, -1 on failure
 */
int stm32_profiler_init(void);

/**
 * \brief           Reset profiler statistics
 */
void stm32_profiler_reset(void);

/**
 * \brief           Start profiling a zone
 * \param[in]       name: Zone name (must be string literal)
 * \return          Zone ID, or -1 on failure
 */
int stm32_profiler_zone_begin(const char* name);

/**
 * \brief           End profiling a zone
 * \param[in]       zone_id: Zone ID returned by zone_begin
 */
void stm32_profiler_zone_end(int zone_id);

/**
 * \brief           Get zone statistics
 * \param[in]       zone_id: Zone ID
 * \param[out]      zone: Pointer to zone structure
 * \return          0 on success, -1 on failure
 */
int stm32_profiler_get_zone(int zone_id, stm32_profile_zone_t* zone);

/**
 * \brief           Get profiler statistics
 * \param[out]      stats: Pointer to statistics structure
 * \return          0 on success, -1 on failure
 */
int stm32_profiler_get_stats(stm32_profiler_stats_t* stats);

/**
 * \brief           Print profiler report
 */
void stm32_profiler_report(void);

/**
 * \brief           Print profiler report sorted by total time
 */
void stm32_profiler_report_sorted(void);

/*---------------------------------------------------------------------------*/
/* Profiler Macros                                                           */
/*---------------------------------------------------------------------------*/

#ifdef NX_CONFIG_STM32_ENABLE_PROFILING

/**
 * \brief           Profile a code block
 * \param[in]       name: Zone name
 *
 * Usage:
 *   STM32_PROFILE_ZONE("my_function") {
 *       // Code to profile
 *   }
 */
#define STM32_PROFILE_ZONE(name)                                               \
    for (int _zone_id = stm32_profiler_zone_begin(name), _done = 0; !_done;    \
         stm32_profiler_zone_end(_zone_id), _done = 1)

/**
 * \brief           Profile a function
 * \param[in]       name: Zone name
 *
 * Usage:
 *   void my_function(void) {
 *       STM32_PROFILE_FUNCTION("my_function");
 *       // Function body
 *   }
 */
#define STM32_PROFILE_FUNCTION(name)                                           \
    int _zone_id = stm32_profiler_zone_begin(name);                            \
    __attribute__((cleanup(_stm32_profiler_cleanup))) int* _zone_ptr = &_zone_id

/* Cleanup helper for PROFILE_FUNCTION */
static inline void _stm32_profiler_cleanup(int* zone_id) {
    if (zone_id && *zone_id >= 0) {
        stm32_profiler_zone_end(*zone_id);
    }
}

#else /* !NX_CONFIG_STM32_ENABLE_PROFILING */

/* Profiling disabled - macros expand to nothing */
#define STM32_PROFILE_ZONE(name) if (1)
#define STM32_PROFILE_FUNCTION(name)                                           \
    do {                                                                       \
    } while (0)

#endif /* NX_CONFIG_STM32_ENABLE_PROFILING */

/*---------------------------------------------------------------------------*/
/* Hot Path Analysis                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Identify hot paths (most time-consuming zones)
 * \param[out]      hot_zones: Array to store hot zone IDs
 * \param[in]       max_zones: Maximum number of zones to return
 * \return          Number of hot zones found
 */
int stm32_profiler_find_hot_paths(int* hot_zones, int max_zones);

/**
 * \brief           Print hot path analysis
 * \param[in]       top_n: Number of top zones to display
 */
void stm32_profiler_hot_path_report(int top_n);

#ifdef __cplusplus
}
#endif

#endif /* STM32_PROFILER_H */

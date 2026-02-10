/**
 * \file            stm32_profiler.c
 * \brief           STM32 advanced performance profiler implementation
 * \author          Nexus Team
 */

#include "system/stm32_profiler.h"
#include "system/stm32_performance.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Profiler State                                                            */
/*---------------------------------------------------------------------------*/

static stm32_profile_zone_t g_zones[STM32_PROFILER_MAX_ZONES];
static uint32_t g_zone_count = 0;
static uint32_t g_total_samples = 0;
static uint32_t g_overflow_count = 0;
static bool g_profiler_initialized = false;

/*---------------------------------------------------------------------------*/
/* Profiler Functions                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize profiler
 * \details         Initializes DWT and resets profiler state
 */
int stm32_profiler_init(void) {
    /* Initialize DWT for cycle counting */
    if (stm32_perf_init() != 0) {
        return -1;
    }

    /* Reset profiler state */
    stm32_profiler_reset();

    g_profiler_initialized = true;
    return 0;
}

/**
 * \brief           Reset profiler statistics
 */
void stm32_profiler_reset(void) {
    memset(g_zones, 0, sizeof(g_zones));
    g_zone_count = 0;
    g_total_samples = 0;
    g_overflow_count = 0;
}

/**
 * \brief           Find or create a zone
 */
static int find_or_create_zone(const char* name) {
    /* Search for existing zone */
    for (uint32_t i = 0; i < g_zone_count; i++) {
        if (g_zones[i].name == name) {
            return i;
        }
    }

    /* Create new zone */
    if (g_zone_count >= STM32_PROFILER_MAX_ZONES) {
        g_overflow_count++;
        return -1;
    }

    int zone_id = g_zone_count++;
    g_zones[zone_id].name = name;
    g_zones[zone_id].call_count = 0;
    g_zones[zone_id].total_cycles = 0;
    g_zones[zone_id].min_cycles = 0xFFFFFFFF;
    g_zones[zone_id].max_cycles = 0;
    g_zones[zone_id].avg_cycles = 0;
    g_zones[zone_id].active = false;

    return zone_id;
}

/**
 * \brief           Start profiling a zone
 */
int stm32_profiler_zone_begin(const char* name) {
    if (!g_profiler_initialized || name == NULL) {
        return -1;
    }

    int zone_id = find_or_create_zone(name);
    if (zone_id < 0) {
        return -1;
    }

    /* Record start cycle */
    g_zones[zone_id].start_cycle = stm32_perf_get_cycles();
    g_zones[zone_id].active = true;

    return zone_id;
}

/**
 * \brief           End profiling a zone
 */
void stm32_profiler_zone_end(int zone_id) {
    if (!g_profiler_initialized || zone_id < 0 ||
        zone_id >= (int)g_zone_count) {
        return;
    }

    stm32_profile_zone_t* zone = &g_zones[zone_id];

    if (!zone->active) {
        return;
    }

    /* Calculate elapsed cycles */
    uint32_t end_cycle = stm32_perf_get_cycles();
    uint32_t elapsed;

    if (end_cycle >= zone->start_cycle) {
        elapsed = end_cycle - zone->start_cycle;
    } else {
        /* Handle overflow */
        elapsed = (0xFFFFFFFF - zone->start_cycle) + end_cycle + 1;
    }

    /* Update statistics */
    zone->call_count++;
    zone->total_cycles += elapsed;

    if (elapsed < zone->min_cycles) {
        zone->min_cycles = elapsed;
    }

    if (elapsed > zone->max_cycles) {
        zone->max_cycles = elapsed;
    }

    zone->avg_cycles = zone->total_cycles / zone->call_count;
    zone->active = false;

    g_total_samples++;
}

/**
 * \brief           Get zone statistics
 */
int stm32_profiler_get_zone(int zone_id, stm32_profile_zone_t* zone) {
    if (!g_profiler_initialized || zone == NULL || zone_id < 0 ||
        zone_id >= (int)g_zone_count) {
        return -1;
    }

    *zone = g_zones[zone_id];
    return 0;
}

/**
 * \brief           Get profiler statistics
 */
int stm32_profiler_get_stats(stm32_profiler_stats_t* stats) {
    if (!g_profiler_initialized || stats == NULL) {
        return -1;
    }

    stats->total_zones = g_zone_count;
    stats->total_samples = g_total_samples;
    stats->overflow_count = g_overflow_count;

    /* Count active zones */
    stats->active_zones = 0;
    for (uint32_t i = 0; i < g_zone_count; i++) {
        if (g_zones[i].active) {
            stats->active_zones++;
        }
    }

    return 0;
}

/**
 * \brief           Print profiler report
 */
void stm32_profiler_report(void) {
    if (!g_profiler_initialized) {
        printf("Profiler not initialized\n");
        return;
    }

    printf("\n");
    printf("=== Profiler Report ===\n");
    printf("Total zones: %u\n", (unsigned int)g_zone_count);
    printf("Total samples: %u\n", (unsigned int)g_total_samples);

    if (g_overflow_count > 0) {
        printf("Overflows: %u (increase STM32_PROFILER_MAX_ZONES)\n",
               (unsigned int)g_overflow_count);
    }

    printf("\n");
    printf("%-30s %10s %10s %10s %10s %10s\n", "Zone", "Calls", "Total(us)",
           "Min(us)", "Max(us)", "Avg(us)");
    printf("-------------------------------------------------------------------"
           "-------------\n");

    for (uint32_t i = 0; i < g_zone_count; i++) {
        stm32_profile_zone_t* zone = &g_zones[i];

        if (zone->call_count == 0) {
            continue;
        }

        uint32_t total_us = stm32_perf_cycles_to_us(zone->total_cycles);
        uint32_t min_us = stm32_perf_cycles_to_us(zone->min_cycles);
        uint32_t max_us = stm32_perf_cycles_to_us(zone->max_cycles);
        uint32_t avg_us = stm32_perf_cycles_to_us(zone->avg_cycles);

        printf("%-30s %10u %10u %10u %10u %10u\n", zone->name,
               (unsigned int)zone->call_count, (unsigned int)total_us,
               (unsigned int)min_us, (unsigned int)max_us,
               (unsigned int)avg_us);
    }

    printf("==================================================================="
           "=============\n\n");
}

/**
 * \brief           Compare zones by total cycles (for sorting)
 */
static int compare_zones_by_total(const void* a, const void* b) {
    const stm32_profile_zone_t* zone_a = (const stm32_profile_zone_t*)a;
    const stm32_profile_zone_t* zone_b = (const stm32_profile_zone_t*)b;

    if (zone_b->total_cycles > zone_a->total_cycles) {
        return 1;
    } else if (zone_b->total_cycles < zone_a->total_cycles) {
        return -1;
    }
    return 0;
}

/**
 * \brief           Print profiler report sorted by total time
 */
void stm32_profiler_report_sorted(void) {
    if (!g_profiler_initialized) {
        printf("Profiler not initialized\n");
        return;
    }

    /* Create a copy of zones for sorting */
    stm32_profile_zone_t sorted_zones[STM32_PROFILER_MAX_ZONES];
    memcpy(sorted_zones, g_zones, sizeof(g_zones));

    /* Sort by total cycles (descending) */
    qsort(sorted_zones, g_zone_count, sizeof(stm32_profile_zone_t),
          compare_zones_by_total);

    printf("\n");
    printf("=== Profiler Report (Sorted by Total Time) ===\n");
    printf("Total zones: %u\n", (unsigned int)g_zone_count);
    printf("Total samples: %u\n", (unsigned int)g_total_samples);
    printf("\n");
    printf("%-30s %10s %10s %10s %10s %10s\n", "Zone", "Calls", "Total(us)",
           "Min(us)", "Max(us)", "Avg(us)");
    printf("-------------------------------------------------------------------"
           "-------------\n");

    for (uint32_t i = 0; i < g_zone_count; i++) {
        stm32_profile_zone_t* zone = &sorted_zones[i];

        if (zone->call_count == 0) {
            continue;
        }

        uint32_t total_us = stm32_perf_cycles_to_us(zone->total_cycles);
        uint32_t min_us = stm32_perf_cycles_to_us(zone->min_cycles);
        uint32_t max_us = stm32_perf_cycles_to_us(zone->max_cycles);
        uint32_t avg_us = stm32_perf_cycles_to_us(zone->avg_cycles);

        printf("%-30s %10u %10u %10u %10u %10u\n", zone->name,
               (unsigned int)zone->call_count, (unsigned int)total_us,
               (unsigned int)min_us, (unsigned int)max_us,
               (unsigned int)avg_us);
    }

    printf("==================================================================="
           "=============\n\n");
}

/*---------------------------------------------------------------------------*/
/* Hot Path Analysis                                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Identify hot paths
 */
int stm32_profiler_find_hot_paths(int* hot_zones, int max_zones) {
    if (!g_profiler_initialized || hot_zones == NULL || max_zones <= 0) {
        return 0;
    }

    /* Create a copy of zones for sorting */
    stm32_profile_zone_t sorted_zones[STM32_PROFILER_MAX_ZONES];
    memcpy(sorted_zones, g_zones, sizeof(g_zones));

    /* Sort by total cycles (descending) */
    qsort(sorted_zones, g_zone_count, sizeof(stm32_profile_zone_t),
          compare_zones_by_total);

    /* Find zone IDs in original array */
    int count = 0;
    for (uint32_t i = 0; i < g_zone_count && count < max_zones; i++) {
        if (sorted_zones[i].call_count == 0) {
            continue;
        }

        /* Find zone ID in original array */
        for (uint32_t j = 0; j < g_zone_count; j++) {
            if (g_zones[j].name == sorted_zones[i].name) {
                hot_zones[count++] = j;
                break;
            }
        }
    }

    return count;
}

/**
 * \brief           Print hot path analysis
 */
void stm32_profiler_hot_path_report(int top_n) {
    if (!g_profiler_initialized) {
        printf("Profiler not initialized\n");
        return;
    }

    if (top_n <= 0 || top_n > (int)g_zone_count) {
        top_n = g_zone_count;
    }

    int hot_zones[STM32_PROFILER_MAX_ZONES];
    int count = stm32_profiler_find_hot_paths(hot_zones, top_n);

    printf("\n");
    printf("=== Hot Path Analysis (Top %d) ===\n", count);
    printf("\n");

    uint32_t total_cycles = 0;
    for (uint32_t i = 0; i < g_zone_count; i++) {
        total_cycles += g_zones[i].total_cycles;
    }

    printf("%-30s %10s %10s %10s\n", "Zone", "Total(us)", "Calls", "% Time");
    printf("-------------------------------------------------------------------"
           "-------------\n");

    for (int i = 0; i < count; i++) {
        int zone_id = hot_zones[i];
        stm32_profile_zone_t* zone = &g_zones[zone_id];

        uint32_t total_us = stm32_perf_cycles_to_us(zone->total_cycles);
        float percent = (total_cycles > 0)
                            ? (100.0f * zone->total_cycles / total_cycles)
                            : 0.0f;

        printf("%-30s %10u %10u %9.2f%%\n", zone->name, (unsigned int)total_us,
               (unsigned int)zone->call_count, percent);
    }

    printf("==================================================================="
           "=============\n");
    printf("\nOptimization recommendations:\n");
    printf("  1. Focus on zones with highest %% Time\n");
    printf("  2. Reduce call count for frequently called zones\n");
    printf("  3. Optimize algorithms in hot zones\n");
    printf("  4. Consider caching results\n");
    printf("  5. Use DMA for data transfers\n");
    printf("\n");
}

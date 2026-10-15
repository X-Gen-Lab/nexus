/**
 * \file            adc_observer.h
 * \brief           Optional caller-owned ADC publication observations
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ADC_OBSERVER_H
#define NEXUS_IO_ADC_OBSERVER_H

#include "nexus/core/time.h"
#include "nexus/io/stream.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NX_ADC_OBSERVATION_DISCONTINUITY 1U

/**
 * \brief           One loan's software observation, never a sample aperture.
 * \note            observed_at uses the platform monotonic clock after target
 *                  detach readback. IRQ delivery and publication latency are
 *                  included. sequence counts observed valid block records,
 *                  not timer triggers, samples or guessed missed scans.
 */
typedef struct {
    uint64_t epoch;
    nx_time_us_t observed_at;
    uint64_t sequence;
    uint32_t flags;
} nx_adc_observation_t;

/**
 * \brief           Optional caller-owned observer control and exact records.
 * \note            Initialize before attaching; internal fields belong to one
 *                  producer. Records and control remain live through successful
 *                  provider stop and all consumer releases. No default stream,
 *                  slot or finite provider storage is enlarged.
 */
typedef struct {
    nx_stream_t* stream;
    nx_adc_observation_t* records;
    size_t count;
    uint64_t sequence;
    bool discontinuity;
} nx_adc_observer_t;

/**
 * \brief           Initialize fresh unborrowed optional observation storage.
 * \param[out]      observer: Aligned fresh control, not published to readers.
 * \param[out]      records: Distinct aligned exact record array.
 * \param[in]       count: Positive count; must equal the selected stream count.
 * \return          Success or INVALID/CONTEXT; rejected outputs stay untouched.
 */
nx_result_t nx_adc_observer_initialize(nx_adc_observer_t* observer,
                                       nx_adc_observation_t* records,
                                       size_t count);
/**
 * \brief           Provider-only cold bind validates every metadata/payload
 *                  span before the first DMA loan.
 * \param[in,out]   observer: Initialized exclusive observation control.
 * \param[in]       stream: Exact initialized stream, no outstanding loans.
 * \param[in]       owner: Provider's stable complete private state span.
 * \param[in]       owner_size: Positive exact state byte count.
 * \return          Success or INVALID/BUSY, without a partial retained bind.
 */
nx_result_t nx_adc_observer_bind(nx_adc_observer_t* observer,
                                 nx_stream_t* stream, const void* owner,
                                 size_t owner_size);
/**
 * \brief           Provider-only record before matching release-publication.
 * \param[in,out]   observer: Bound exact producer-owned control.
 * \param[in]       fill: Still FILLING, with a proved detached memory target.
 * \param[in]       observed_at: Software observation in the same clock domain.
 * \return          Success or STATE/EXHAUSTED without wrapping identities.
 */
nx_result_t nx_adc_observer_record(nx_adc_observer_t* observer,
                                   const nx_stream_fill_t* fill,
                                   nx_time_us_t observed_at);
/**
 * \brief           Read only a currently acquired block's exact epoch record.
 * \param[in]       observer: Stable bound observation control.
 * \param[in]       block: Same stream's live immutable consumer loan.
 * \param[out]      observation: Complete copy; untouched on rejection.
 * \return          Success or INVALID/STATE for a missing/stale/released loan.
 */
nx_result_t nx_adc_observer_read(const nx_adc_observer_t* observer,
                                 const nx_stream_block_t* block,
                                 nx_adc_observation_t* observation);
/**
 * \brief           Provider-only unbind after all producer/consumer loans join.
 * \param[in,out]   observer: Stable initialized control; NULL is allowed.
 * \return          Success or BUSY retaining the bound stream.
 */
nx_result_t nx_adc_observer_unbind(nx_adc_observer_t* observer);
#ifdef __cplusplus
}
#endif
#endif

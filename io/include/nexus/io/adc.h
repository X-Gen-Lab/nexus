/**
 * \file            adc.h
 *
 * \brief           Reviewed ADC single-shot and low-rate polling scans.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ADC_H
#define NEXUS_ADC_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_adc_port nx_adc_port_t;
/**
 * \brief           Raw count facts; nominal Vref is not a calibrated physical
 *                  measurement.
 */
typedef struct {
    uint8_t resolution_bits;
    uint32_t reference_mv;
    size_t channel_count;
} nx_adc_info_t;
/**
 * \brief           Query the fixed reviewed ADC sequence and raw-count format.
 *
 * \param[in]       port: Single-shot/low-rate scan provider.
 *
 * \param[out]      info: Resolution, declared reference and sequence length.
 *
 * \return          Success or INVALID.
 */
nx_result_t nx_adc_port_info(const nx_adc_port_t* port, nx_adc_info_t* info);
/**
 * \brief           Sample the fixed single/scan sequence by bounded polling.
 *
 * \param[in,out]   port: Single execution owner; fixed channel/sample-time
 *                  setup.
 *
 * \param[out]      samples: Raw counts in configured channel order.
 *
 * \param[in]       capacity: Sample capacity at least channel_count.
 *
 * \param[in]       deadline: Absolute clock-domain deadline.
 *
 * \param[out]      count: Complete valid samples; remaining output is
 *                  untouched.
 *
 * \return          Success or INVALID/BUSY/TIMEOUT/IO; abort/drain completes
 *                  before return. No retained buffer, DMA/trigger/stream
 *                  promise.
 *
 * \note            CPU-active task-only acquisition. Reference voltage,
 *                  sampling error and maximum rate need physical
 *                  qualification.
 */
nx_result_t nx_adc_port_sample(nx_adc_port_t* port, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count);
#ifdef __cplusplus
}
#endif

#endif

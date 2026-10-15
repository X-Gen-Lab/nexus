/**
 * \file            bmp280.h
 * \brief           BMP280 calibration and explicit forced-conversion driver
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_BMP280_H
#define NEXUS_COMPONENTS_BMP280_H
#include "nexus/core/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Synchronous settled SPI transaction port
 * \note            exchange always releases every TX/RX buffer before
 *                  returning, including TIMEOUT/error. One CS interval covers
 *                  all bytes; mode 0 or 3, 8-bit words, <=10 MHz. Caller
 *                  serializes this device and its bus owner. Driver stack
 *                  transactions never cross asynchronous borrowing boundaries.
 */
typedef struct {
    void* context;
    nx_result_t (*exchange)(void*, const uint8_t*, uint8_t*, size_t,
                            nx_time_us_t deadline);
    nx_clock_t clock;
} nx_bmp280_transport_port_t;

/** \brief Register calibration, owned by one device instance. */
typedef struct {
    uint16_t t1;
    int16_t t2;
    int16_t t3;
    uint16_t p1;
    int16_t p2;
    int16_t p3;
    int16_t p4;
    int16_t p5;
    int16_t p6;
    int16_t p7;
    int16_t p8;
    int16_t p9;
} nx_bmp280_calibration_t;

/** \brief Output is valid only on successful conversion/compensation. */
typedef struct {
    int32_t centi_celsius;
    uint32_t pressure_pa;
} nx_bmp280_sample_t;

/** \brief Explicit device context, no Board pins or automatic sampling task. */
typedef struct {
    nx_bmp280_transport_port_t transport;
    nx_bmp280_calibration_t calibration;
    nx_time_us_t deadline;
    nx_time_us_t earliest_ready;
    bool initialized;
    bool measuring;
} nx_bmp280_t;

/**
 * \brief           Parse the exact 24-byte little-endian calibration block
 * \param[in]       bytes: Register block beginning at 0x88
 * \param[in]       length: Must be exactly 24
 * \param[out]      calibration: Written only on success
 * \return          SUCCESS or INVALID for length/zero pressure divisor
 */
nx_result_t nx_bmp280_parse_calibration(const uint8_t* bytes, size_t length,
                                        nx_bmp280_calibration_t* calibration);
/**
 * \brief           Compensate one raw sample using checked integer arithmetic
 * \param[in]       calibration: Parsed calibration
 * \param[in]       raw_temperature: Unsigned 20-bit ADC value
 * \param[in]       raw_pressure: Unsigned 20-bit ADC value
 * \param[out]      sample: Written only on success
 * \return          SUCCESS, INVALID for malformed/overflow calibration or IO
 *                  for disabled ADC sentinel/out-of-datasheet-range physical
 *                  output
 */
nx_result_t nx_bmp280_compensate(const nx_bmp280_calibration_t* calibration,
                                 uint32_t raw_temperature,
                                 uint32_t raw_pressure,
                                 nx_bmp280_sample_t* sample);
/**
 * \brief           Validate chip ID and load calibration, without scanning
 * \param[out]      device: Zero-initialized unused instance
 * \param[in]       transport: Synchronous settled port and live monotonic clock
 * \param[in]       deadline: Absolute deadline for all initialization accesses
 * \return          SUCCESS, BUSY for NVM update, or transport/identity error
 * \note            Task context; failure leaves initialized false and no
 *                  borrow. Does not soft-reset the part or create a wait
 *                  loop/task. Retry BUSY explicitly.
 */
nx_result_t nx_bmp280_init(nx_bmp280_t* device,
                           nx_bmp280_transport_port_t transport,
                           nx_time_us_t deadline);
/**
 * \brief           Start one forced temperature/pressure conversion
 * \param[in,out]   device: Initialized non-measuring instance
 * \param[in]       deadline: Absolute deadline covering conversion and reads
 * \return          SUCCESS, BUSY, TIMEOUT or transport error
 * \note            Task-only serialized driver owner; oversampling x1/x1.
 */
nx_result_t nx_bmp280_begin(nx_bmp280_t* device, nx_time_us_t deadline);
/**
 * \brief           Poll conversion without an implicit worker or blocking delay
 * \param[in,out]   device: Instance with a previously started conversion
 * \param[out]      sample: Written only on SUCCESS
 * \return          SUCCESS, BUSY while converting, TIMEOUT or transport error
 * \note            Task-only. Every port call releases temporary buffers before
 *                  return. BUSY retains device conversion state, never stack or
 *                  caller buffers. After terminal failure, begin may retry; the
 *                  external owner chooses policy.
 */
nx_result_t nx_bmp280_poll(nx_bmp280_t* device, nx_bmp280_sample_t* sample);
/**
 * \brief           Stop software use after all synchronous calls have exited
 * \param[in,out]   device: Quiesced driver instance
 * \note            Does not power off or reset a physical part. Sensor-internal
 *                  conversion can finish without accessing MCU storage. No MCU
 *                  borrow remains.
 */
void nx_bmp280_stop(nx_bmp280_t* device);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_COMPONENTS_BMP280_H */

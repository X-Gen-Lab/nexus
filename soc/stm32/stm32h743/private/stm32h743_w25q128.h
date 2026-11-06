/**
 * \file            stm32h743_w25q128.h
 * \brief           Private bounded H743 single-line W25Q128JV reference
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32H743_W25Q128_H
#define NX_STM32H743_W25Q128_H
#include "nexus/components/mapped_storage_owner.h"
#include "stm32h7xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NX_H743_W25Q128_BYTES       16777216U
#define NX_H743_W25Q128_PAGE_BYTES  256U
#define NX_H743_W25Q128_ERASE_BYTES 4096U
#define NX_H743_W25Q128_CHECK_BYTES 16U
#define NX_H743_W25Q128_MAX_HZ      16000000U

/** \brief Two explicitly leased MPU regions; deny has lower priority. */
typedef struct {
    uint8_t deny_region;
    uint8_t read_region;
} nx_h743_w25q128_mpu_config_t;

/** \brief One private HAL/controller/protocol owner, without heap or IRQ work.
 * \note The SoC cold owner acquires exact clock and pin leases first and keeps
 * them stable through stop. This reference requires both CPU caches disabled,
 * QUADSPI NVIC disabled/inactive, no DMA/XIP/external controller user and an
 * already powered stable flash. No PCB, route or electrical fact is inferred.
 * Two exclusive static MPU leases cover the complete256MiB aperture with a
 * lower deny/XN region and a higher16MiB read-only/XN/noncache region. Startup
 * already enables MPU and HFNMIENA; CTRL and other overriding policy remain
 * stable. This provider never enables a global MPU or guesses unused slots.
 * HAL peripheral interrupt facts are dispatched only by the serialized task.
 */
typedef struct {
    QSPI_HandleTypeDef hal;
    nx_clock_t clock;
    const uint8_t* input;
    size_t offset;
    size_t length;
    size_t completed;
    size_t chunk;
    size_t checked;
    nx_time_us_t deadline;
    nx_time_us_t pulse_deadline;
    nx_time_us_t power_until;
    nx_time_us_t startup_deadline;
    uint32_t mpu_control;
    uint32_t mpu_selector;
    uint32_t mpu_work_selector;
    uint32_t mpu_base[2];
    uint32_t mpu_attributes[2];
    nx_result_t result;
    uint8_t response[NX_H743_W25Q128_CHECK_BYTES];
    uint8_t phase;
    uint8_t command;
    uint8_t operation;
    uint8_t status1;
    uint8_t status2;
    uint8_t status3;
    uint8_t mpu_deny_region;
    uint8_t mpu_read_region;
    uint8_t mpu_effects;
    bool owned;
    bool ready;
    bool mapped;
    bool active;
    bool pending;
    bool cancelled;
    bool pulse;
    bool aborting;
    bool abort_requested;
    bool transport_error;
    bool mpu_readable;
} nx_h743_w25q128_state_t;

/** \brief Acquire only a cold, reset, exclusively leased controller.
 * \return SUCCESS starts discovery; status remains BUSY until service finishes.
 * \note Finite 5ms power-up guard precedes discovery. Only EF4018 Q/N/JQ
 * identity, unprotected array, mode0/1-1-1/24bit and <=16MHz are admitted.
 * No status/QE/reset/suspend command is issued. Failure after effects keeps
 * owned true and requires explicit stop; state/clock outlive all service calls.
 * Both claimed regions must be disabled initially; read_region>deny_region.
 * Stop restores their effective BASE/attributes and original selector after
 * every view, request and controller source has joined.
 */
nx_result_t nx_h743_w25q128_prepare(nx_h743_w25q128_state_t* state,
                                    nx_clock_t clock, uint32_t kernel_hz,
                                    uint32_t serial_hz,
                                    nx_h743_w25q128_mpu_config_t mpu);
/** \brief Advance one finite command/FIFO step on the unmasked task owner. */
void nx_h743_w25q128_service(nx_h743_w25q128_state_t* state);
/** \brief Query discovery without creating a read map or starting a command. */
nx_result_t nx_h743_w25q128_status(const nx_h743_w25q128_state_t* state);
/** \brief Export the existing backend ABI only after verified discovery.
 * \note Output is untouched on rejection. CPU_READ alone is advertised;
 * program_unit1 differs from the physical256-byte page chunk. Generic owner
 * slots, face and clock are separately caller-owned and must not alias state.
 */
nx_result_t nx_h743_w25q128_backend(nx_h743_w25q128_state_t* state,
                                    nx_mapped_storage_backend_t* backend);
/** \brief Stop only after generic owner stop, loan and request/consumer join.
 * \note BUSY/IO retains private ownership and allows an explicit retry. It does
 * not revoke a map/request, disable a shared clock or restore unknown pin
 * state.
 * After failed last-map release, consumers remain permanently joined: the
 * guard may already deny the old view. Only cleanup retry is authorized.
 */
nx_result_t nx_h743_w25q128_stop(nx_h743_w25q128_state_t* state);
/** \brief Private HAL zero-timeout bridge; not a second platform SysTick. */
uint32_t nx_h743_w25q128_hal_get_tick(void);
#ifdef __cplusplus
}
#endif
#endif

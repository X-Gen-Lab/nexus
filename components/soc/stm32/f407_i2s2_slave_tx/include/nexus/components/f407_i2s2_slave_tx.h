/**
 * \file            f407_i2s2_slave_tx.h
 * \brief           Exact F407 finite stereo16 I2S2 slave transmitter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_F407_I2S2_SLAVE_TX_H
#define NEXUS_COMPONENTS_F407_I2S2_SLAVE_TX_H

#include "nexus/core/request.h"
#include "nexus/io/frame_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           One complete contiguous interleaved stereo16 frame block.
 * \note            Initialize and prepare base with the ordinary helpers.
 *                  The source has one plane of contiguous nonempty rows, no
 *                  offset or padding, and 4..131068 bytes in a multiple of
 *                  four. Each frame stores
 *                  left then right 16-bit little-endian samples. The fixed
 *                  Philips I2S wire emits each sample MSB first in 16-bit
 * slots. Sample values, rate, codec and external clocks are caller policy.
 * Acceptance transfers the exact immutable source loan and borrows this request
 * until acquire-observed SETTLED. Never separately release, mutate or reuse
 * accepted storage. SUCCESS reports the whole block only after memory and wire
 * completion. Every error or cancellation reports zero; the DMA count does not
 * establish serialized audio bytes.
 */
typedef struct {
    nx_request_t base;
    nx_frame_dma_loan_t* source;
} nx_f407_i2s2_tx_request_t;

/**
 * \brief           One privileged serialized task executor; no ISR callback.
 * \note            submit SUCCESS means ACCEPTED, including subsequent setup
 *                  failure. QUEUED is not armed. Acquire-observed ACTIVE alone
 *                  authorizes the external master to begin CK, after one DR
 *                  halfword and a live TIM2 interval have been observed.
 *                  Error, cancellation or timeout retains the full loan until
 *                  actual engine, serializer, vector and address withdrawal.
 *                  service returns SUCCESS when idle or settled, otherwise
 *                  BUSY or an observed failure with the borrow retained. stop
 *                  closes further admission and requests cancellation; repeat
 *                  it after service proves the remaining cleanup. The first
 *                  SR observation occurs only after acceptance because reading
 *                  SR clears UDR/FRE. Its fault remains latched through
 * cleanup; a later clean status read cannot erase the operation's cause. No
 * method allocates, starts a task or replays a block.
 *                  Admission checks controls against every full-capacity pool
 *                  slot in O(slot count); service/cancel do not scan the pool.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_f407_i2s2_tx_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*cancel)(void* context, nx_f407_i2s2_tx_request_t* request);
    nx_result_t (*stop)(void* context);
} nx_f407_i2s2_slave_tx_ops_t;

/** \brief Immutable caller-owned face; private state is separate. */
typedef struct {
    const nx_f407_i2s2_slave_tx_ops_t* ops;
    void* context;
} nx_f407_i2s2_slave_tx_t;

/** \brief Fixed chip resource selected by the same validated hardware IR. */
typedef enum { NX_F407_I2S2_SLAVE_TX = 1 } nx_f407_i2s2_resource_t;

/**
 * \brief           Exact resource and positive bounded observation budget.
 * \note            poll_limit bounds each arm/drain observation loop, not time
 *                  or external-clock progress. Arming observes at least three
 *                  actual 1 MHz TIM2 ticks after I2SE readback. Deadlines are
 *                  absolute in the current live Platform TIM2 epoch. The caller
 *                  stops and joins this owner before Platform stop/restart;
 *                  the deadline includes wire and observer withdrawal. No
 *                  automatic ClockDomain enrollment is implied.
 */
typedef struct {
    nx_f407_i2s2_resource_t resource;
    uint32_t poll_limit;
} nx_f407_i2s2_slave_tx_config_t;

#if UINTPTR_MAX == UINT32_MAX
#define NX_F407_I2S2_SLAVE_TX_STORAGE_BYTES 88U
#elif UINTPTR_MAX == UINT64_MAX
#define NX_F407_I2S2_SLAVE_TX_STORAGE_BYTES 120U
#else
#error "F407 I2S2 maintains 32-bit firmware and 64-bit host-model ABIs"
#endif
/**
 * \brief           Exact storage for one owner, zeroed once before publication.
 * \note            Do not copy or alter. Keep storage and face live through
 *                  successful stop. Accepted request/source loans last until
 *                  acquire-observed SETTLED; a failed stop never revokes an
 *                  unfinished borrow.
 */
typedef union {
    uint64_t alignment;
    uintptr_t pointer_alignment;
    unsigned char private_bytes[NX_F407_I2S2_SLAVE_TX_STORAGE_BYTES];
} nx_f407_i2s2_slave_tx_storage_t;

/** \brief Shared immutable methods; callbacks receive the storage context. */
extern const nx_f407_i2s2_slave_tx_ops_t nx_f407_i2s2_slave_tx_ops;

/**
 * \brief           Acquire the selected F407 SPI2 personality and fixed pads.
 * \param[in,out]   storage: Fresh zero storage, disjoint from all controls.
 * \param[in]       face: Stable readonly {&nx_f407_i2s2_slave_tx_ops, storage}.
 * \param[in]       config: Fixed resource and observation budget.
 * \return          SUCCESS, INVALID, CONTEXT, BUSY, STATE, UNSUPPORTED or IO.
 *                  Initialization
 *                  IO retains cleanup responsibility; call stop before reuse.
 * \note            Exact F407VE/ZG, baremetal, HSE8/PLL168 and live TIM2 only.
 *                  The normal hardware role exclusively claims SPI2/I2S2,
 *                  PB12 WS/PB13 CK/PB15 SD AF5, DMA1 Stream4 channel0 and both
 *                  vectors. No PLLI2S, continuous DBM or IRQ service is
 * enabled. Peripheral/pad/SPI2 gate ownership lasts until stop SUCCESS,
 *                  independently of each request's shorter frame loan.
 *                  The selected cold-reset Platform separately holds the shared
 *                  GPIOB/DMA1 gates. Initialization requires both already on;
 *                  this provider never enables or withdraws them. Every shared
 *                  domain user, including external HAL users, must join before
 *                  Platform stop. Clock-gating is not proof of a reset engine;
 *                  unknown running-state attachment is unsupported.
 *
 *                  The caller owns the external master and holds CK idle LOW
 * and WS HIGH continuously from before initialization/submission until
 * acquire-observed ACTIVE. Pin reads sample instants; they cannot prove
 * electrical silence. The master transmits exactly the declared complete frames
 * and stops before a missing next word. Restart begins at the left channel. A
 * prefilling cancellation with CK idle cannot empty DR; preserve QUARANTINED
 * storage until coordinated drain clocks allow TXE then fresh BSY observations.
 * This error drain can underrun and does not promise valid stereo audio or a
 * complete transmitted frame; errors report zero transferred bytes.
 * Timeout, TC or NDTR0 never proves wire completion or final access.
 *                  The caller also supplies valid operating voltage, I/O
 *                  levels, load, CK/WS/SD timing and DMA refill margin under
 *                  DS8626. ACTIVE and pin samples do not measure or qualify
 *                  those electrical/rate conditions.
 *
 *                  One privileged unmasked task serializes every method and
 *                  external owner lifecycle. Preserve incoming masks on all
 *                  rejection paths. NMI/HardFault, other DMA/IRQ/pin owners,
 *                  reset/reclock/debugger writers and concurrent Platform stop
 *                  are excluded. Observed reset or clock-profile history loss
 *                  while borrowed permanently retains the request; restoring
 *                  reset-looking registers cannot establish prior final access.
 *                  Main SRAM is DMA-visible; CCM is not. Chip pin capability
 *                  establishes neither a PCB route nor codec/electrical HIL.
 *                  The assembly and fixture supply the actual F407/package
 *                  identity: shared device-group and density reads alone do not
 *                  distinguish an F405 or authenticate the physical package.
 */
nx_result_t
nx_f407_i2s2_slave_tx_initialize(nx_f407_i2s2_slave_tx_storage_t* storage,
                                 const nx_f407_i2s2_slave_tx_t* face,
                                 const nx_f407_i2s2_slave_tx_config_t* config);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_F407_I2S2_SLAVE_TX_H */

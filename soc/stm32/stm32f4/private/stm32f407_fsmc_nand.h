/**
 * \file            stm32f407_fsmc_nand.h
 * \brief           Raw NAND state sharing the sole FSMC owner and bank receipt
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_FSMC_NAND_H
#define NEXUS_STM32F407_FSMC_NAND_H
#include "nexus/io/nand.h"
#include "stm32f407_fsmc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Explicit raw geometry, no auto-discovery or product bad-block data.
 */
typedef struct {
    const nx_nand_geometry_t* geometry;
    /** Explicit tWB/read/reset busy assertion bound plus one Core us margin.
     * Zero is reserved for externally proven zero-delay software models. */
    uint32_t ready_delay_us;
} nx_stm32_fsmc_nand_config_t;
/** \brief Static finite request and exact NAND decoder/ECC progress.
 * \note Fresh zero before prepare, internal stable disjoint storage through
 *       cleanup. Configuration and request spans remain alive until real idle.
 */
typedef struct {
    nx_stm32_fsmc_lease_t lease;
    const nx_stm32_fsmc_nand_config_t* config;
    nx_nand_request_t* request;
    nx_time_us_t last_clock;
    nx_time_us_t poll_after;
    size_t done;
    size_t ecc_done;
    nx_result_t terminal;
    uint8_t phase;
    uint8_t bank;
    uint8_t width;
    bool closed;
    bool in_call;
    bool ecc_active;
    bool ecc_pending;
} nx_stm32_fsmc_nand_t;
/** \brief Acquire selected raw NAND decoder and observe device ready.
 * \param[in,out] nand: Fresh zero stable internal endpoint storage.
 * \param[in,out] bus: Prepared sole shared FSMC owner.
 * \param[in] index: Actual selected bank-array index.
 * \param[in] config: Explicit reviewed software geometry/command protocol.
 * \return SUCCESS or retained BUSY/IO after lease acquisition; call close
 *         until actual idle. No reset, erase, page scan or write is implicit.
 * \note Privileged unmasked Thread, serialized with every same-bank face.
 */
nx_result_t
nx_stm32_fsmc_nand_prepare(nx_stm32_fsmc_nand_t* nand, nx_stm32_fsmc_t* bus,
                           size_t index,
                           const nx_stm32_fsmc_nand_config_t* config);
/** \brief Close only after finite request, ECC and physical pulse idle.
 * \param[in,out] nand: Same serialized stable endpoint owner.
 * \return SUCCESS or BUSY/IO retaining exact bank/owner and storage.
 * \note The shared controller and Platform pads stop only after this succeeds.
 */
nx_result_t nx_stm32_fsmc_nand_close(nx_stm32_fsmc_nand_t* nand);
/** \brief Immutable methods, actual context is the selected endpoint state. */
extern const nx_nand_ops_t nx_stm32_fsmc_nand_ops;
#ifdef __cplusplus
}
#endif
#endif

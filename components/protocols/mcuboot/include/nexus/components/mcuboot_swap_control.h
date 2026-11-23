/**
 * \file            mcuboot_swap_control.h
 * \brief           Explicit single-image MCUboot scratch trial/confirm control
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_MCUBOOT_SWAP_CONTROL_H
#define NEXUS_COMPONENTS_MCUBOOT_SWAP_CONTROL_H
#include "nexus/components/mcuboot_control.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Observe the selected scratch profile's next boot action.
 * \param[out]      action: Separate result, unchanged on rejection/error.
 * \return          SUCCESS, INVALID or IO for unknown/PANIC/read error.
 * \note            Task/boot context, serialized by the product. The immutable
 *                  three-area operation, regions, clock and provider live for
 *                  the entire call. NONE is not image authenticity or media
 *                  health; pinned upstream may treat unreadable secondary
 *                  metadata as empty. No allocation, worker or hardware start.
 */
nx_result_t nx_mcuboot_swap_query(nx_mcuboot_action_t* action);
/**
 * \brief           Request a trial or permanent scratch swap via upstream API.
 * \param[in]       request: TEST or PERMANENT; unknown values reject.
 * \return          SUCCESS only after matching action readback; IO or STATE
 *                  may follow partial persistent trailer modification.
 * \note            Same immutable operation and finite deadline as query.
 *                  TEST requires explicit application confirmation after a
 *                  successful boot, otherwise next boot reverts. Product owns
 *                  candidate authentication, health, authorization and reset.
 *                  No request validates the image or renews the deadline.
 */
nx_result_t nx_mcuboot_swap_request(nx_mcuboot_request_t request);
/**
 * \brief           Observe the primary trailer's confirmation flag.
 * \param[out]      confirmed: Written only for a valid SET/UNSET flag.
 * \return          SUCCESS, INVALID, STATE for corrupt flag, or IO.
 * \note            Serialized finite operation; this is a trailer observation,
 *                  not health, executable identity or physical boot evidence.
 */
nx_result_t nx_mcuboot_swap_confirmed(bool* confirmed);
/**
 * \brief           Confirm the current primary through the official API.
 * \return          SUCCESS after SET readback, STATE or IO; partial persistent
 *                  effects remain possible on failure.
 * \note            Product authorizes this only after its health checks and
 *                  exact running-image identification. No automatic confirm,
 *                  reset, replay, Flash rollback or deadline renewal occurs.
 */
nx_result_t nx_mcuboot_swap_confirm(void);
#ifdef __cplusplus
}
#endif
#endif

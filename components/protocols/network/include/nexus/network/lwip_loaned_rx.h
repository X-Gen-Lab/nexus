/**
 * \file            lwip_loaned_rx.h
 * \brief           Explicit immutable RX-payload loan profile for lwIP
 * \author          Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_LOANED_RX_H
#define NEXUS_NETWORK_LWIP_LOANED_RX_H

#include "nexus/network/lwip_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact selected backend slot, defined by its storage target. */
typedef struct nx_lwip_rx_slot nx_lwip_rx_slot_t;
/** \brief Exact selected custom-pbuf piece, not a packet allocator. */
typedef struct nx_lwip_rx_piece nx_lwip_rx_piece_t;

/**
 * \brief Caller-selected RX loan and fragment metadata, without a hidden pool.
 * \note Slots and pieces are zero-initialized and remain live through final
 *       pbuf release, exact MAC release and adapter detachment. Supply at least
 *       slot_count * base.rx_capacity pieces. Each slot owns one copied
 * 136-byte writable header prefix. Remaining packet payload references
 * immutable MAC bytes; raw consumers must not write those PBUF_ROM tail bytes.
 *       The exact provider descriptor geometry still limits concurrent loans.
 */
typedef struct {
    nx_lwip_config_t base;
    nx_lwip_rx_slot_t* slots;
    size_t slot_count;
    nx_lwip_rx_piece_t* pieces;
    size_t piece_count;
} nx_lwip_loaned_config_t;

/**
 * \brief Attach the explicitly selected payload-zero-copy RX profile.
 * \param[in,out] adapter: Zero-initialized selected backend storage.
 * \param[in] config: Disjoint stable storage description; copied on success.
 * \return Success, invalid configuration, or an already attached owner.
 * \note Task-only, same serialized nonreentrant NO_SYS owner as all raw calls.
 *       Select only the loaned profile and its storage target in this image.
 *       Ethernet/ARP/max IPv4/max TCP headers are copied; immutable payload
 *       tails are loaned. This is not whole-frame or TX zero-copy. No VLAN,
 *       IPv6, DHCP, DNS, reassembly, worker, heap or physical link is added.
 *       MAC payload spans must be disjoint from the adapter, TX/view storage
 *       and all selected slot/piece metadata; admission releases invalid loans.
 *       Final custom-pbuf frees only mark a slot ready. Subsequent service or
 *       stop performs exact provider release. Failed release preserves the
 *       token and closes admission. Stop returns BUSY while references remain.
 *       Products close PCBs and all raw observers before adapter withdrawal.
 */
nx_result_t nx_lwip_loaned_adapter_start(nx_lwip_adapter_t* adapter,
                                         const nx_lwip_loaned_config_t* config);

#ifdef __cplusplus
}
#endif
#endif

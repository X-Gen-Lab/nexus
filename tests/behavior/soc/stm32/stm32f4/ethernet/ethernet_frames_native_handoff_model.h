/**
 * \file            ethernet_frames_native_handoff_model.h
 * \brief           External scheduling boundary for a real Native loan reader
 * \author          Nexus Team
 */
#ifndef NEXUS_ETH_FRAMES_NATIVE_HANDOFF_MODEL_H
#define NEXUS_ETH_FRAMES_NATIVE_HANDOFF_MODEL_H
#ifdef __cplusplus
extern "C" {
#endif
void nx_eth_frames_native_before_loan_read(void* mac, const void* request);
#ifdef __cplusplus
}
#endif
#define NX_NATIVE_ETH_FRAMES_BEFORE_LOAN_READ(mac, request)                    \
    nx_eth_frames_native_before_loan_read((mac), (request))
#endif

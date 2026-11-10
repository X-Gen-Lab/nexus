/**
 * \file            peer_main.c
 * \brief           Two selected timer owners with one shared operations table
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/pwm_group.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    if (nx_platform_start().primary != NX_SUCCESS)
        return 1;
    const nx_pwm_group_t* drive =
        nx_factory_pwm_group_ref(NX_PWM_GROUP_REF_DRIVE);
    const nx_pwm_group_t* peer =
        nx_factory_pwm_group_ref(NX_PWM_GROUP_REF_PEER);
    const uint32_t duties[4] = {100U, 200U, 300U, 400U};
    const uint32_t peer_duties[2] = {500U, 750U};
    const nx_pwm_group_frame_t frame = {duties, 4U, 0U};
    const nx_pwm_group_frame_t peer_frame = {peer_duties, 2U, 0U};
    nx_pwm_group_state_t state;
    int status = 0;
    if (drive == NULL || peer == NULL) {
        status = 2;
    } else if (nx_pwm_group_commit(drive, &frame) != NX_SUCCESS ||
               nx_pwm_group_commit(peer, &peer_frame) != NX_SUCCESS ||
               nx_pwm_group_start(drive) != NX_SUCCESS ||
               nx_pwm_group_start(peer) != NX_SUCCESS) {
        status = 3;
    } else if (nx_pwm_group_stop(peer) != NX_SUCCESS ||
               nx_pwm_group_state(drive, &state) != NX_SUCCESS ||
               !state.running) {
        status = 4;
    }
    return nx_platform_stop() == NX_SUCCESS ? status : 5;
}

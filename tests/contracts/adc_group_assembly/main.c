/**
 * \file            main.c
 * \brief           Link-only public finite ADC group and lifecycle consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/adc_group.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static nx_adc_group_request_t s_request;
static nx_adc_group_info_t s_info;
static _Alignas(4) uint16_t s_data[48];

int main(void) {
    const nx_adc_group_port_t* port =
        nx_factory_adc_group_ref(NX_ADC_GROUP_REF_GROUP);
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || port == NULL)
        return 1;
    nx_request_initialize(&s_request.base);
    if (nx_adc_group_info(port, &s_info) != NX_SUCCESS)
        return 2;
    if (nx_adc_group_prepare(&s_request, s_data, s_info.format.scan_stride, 1U,
                             nx_deadline_after(nx_time_now_us(), 100000U)) !=
        NX_SUCCESS)
        return 3;
    (void)nx_adc_group_submit(port, &s_request);
    (void)nx_adc_group_service(port);
    (void)nx_adc_group_cancel(port, &s_request);
    (void)nx_adc_group_stop(port);
    return nx_platform_stop() == NX_SUCCESS ? 0 : 4;
}

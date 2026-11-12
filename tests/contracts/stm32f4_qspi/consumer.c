/**
 * \file            consumer.c
 * \brief           Public QSPI controller/endpoint and linked software contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Link every selected public face without a Flash write/erase pulse. */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        (void)nx_platform_stop();
        return 1;
    }
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_SERIAL);
    const nx_flash_async_port_t* async =
        nx_factory_flash_async_ref(NX_FLASH_ASYNC_REF_STORAGE);
    const nx_flash_port_t* flash =
        nx_factory_flash_async_flash_ref(NX_FLASH_ASYNC_REF_STORAGE);
    if (bus == NULL || async == NULL || flash == NULL ||
        bus->context != async->context || bus->context != flash->context) {
        (void)nx_platform_stop();
        return 2;
    }
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(flash);
    uint32_t sectors = 0U;
    nx_result_t described = nx_flash_geometry_sector_count(geometry, &sectors);
    uint8_t sample = 0xffU;
    nx_flash_async_request_t request;
    nx_request_initialize(&request.base);
    /* Deadline0 is already expired. Admission must reject before any pulse. */
    nx_result_t prepared = nx_flash_async_prepare(
        &request, NX_FLASH_ASYNC_PROGRAM, 0U, &sample, 1U, 0U);
    nx_result_t rejected = nx_flash_async_submit(async, &request);
    nx_result_t cancelled = nx_flash_async_cancel(async, &request);
    nx_result_t progressed = nx_flash_async_service(async);
    nx_result_t io_stopped = nx_flash_async_stop(async);
    nx_result_t read = nx_flash_port_read(flash, 0U, &sample, 1U);
    nx_result_t recovered = nx_flash_port_recover(flash);
    nx_external_bus_state_t retained = nx_external_bus_port_state(bus);
    nx_result_t controller_stopped = nx_external_bus_port_stop(bus);
    nx_result_t platform_stopped = nx_platform_stop();
    return described == NX_SUCCESS && sectors == 4096U &&
                   prepared == NX_SUCCESS && rejected == NX_ERROR_TIMEOUT &&
                   cancelled == NX_ERROR_STATE && progressed == NX_SUCCESS &&
                   io_stopped == NX_SUCCESS && read == NX_SUCCESS &&
                   recovered == NX_SUCCESS &&
                   retained == NX_EXTERNAL_BUS_PREPARED &&
                   controller_stopped == NX_SUCCESS &&
                   platform_stopped == NX_SUCCESS
               ? 0
               : 3;
}

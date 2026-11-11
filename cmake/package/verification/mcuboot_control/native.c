/**
 * \file            native.c
 * \brief           Positive MCUboot control on the public nonpersistent Native
 * NOR model \author          Nexus Team \version         1.0.0 \date 2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "flash_map_backend/flash_map_backend.h"
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include <stdlib.h>

static uint8_t s_memory[8192];
static const nx_flash_sector_t s_sectors[] = {{0U, 4096U}, {4096U, 4096U}};
static const nx_flash_geometry_t s_geometry = {
    0U, sizeof(s_memory), 4U, s_sectors, 2U, 0U};
static nx_flash_region_t s_primary_region;
static nx_flash_region_t s_secondary_region;
static const struct flash_area s_primary = {1U, &s_primary_region};
static const struct flash_area s_secondary = {2U, &s_secondary_region};
static nx_mcuboot_flash_operation_t s_operation;

/** \brief The same live epoch is used by the Native provider and operation. */
static nx_time_us_t read_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}

const nx_mcuboot_flash_operation_t* nx_mcuboot_flash_operation(void) {
    return &s_operation;
}

void nx_mcuboot_assert_fail(void) {
    abort();
}

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    const nx_flash_port_t* flash = nx_factory_flash_ref(NX_FLASH_REF_PHYSICAL);
    if (nx_native_flash_model_configure(flash, s_memory, &s_geometry) !=
        NX_SUCCESS) {
        return 2;
    }
    s_primary_region = (nx_flash_region_t){flash, 0U, 4096U, true};
    s_secondary_region = (nx_flash_region_t){flash, 4096U, 4096U, true};
    s_operation = (nx_mcuboot_flash_operation_t){
        &s_primary,
        &s_secondary,
        {read_clock, NULL},
        nx_deadline_after(read_clock(NULL), 5000000U)};
    nx_result_t result = control_api_run(true);
    s_operation.primary = NULL;
    s_operation.secondary = NULL;
    nx_result_t stopped = nx_platform_stop();
    return result == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 3;
}

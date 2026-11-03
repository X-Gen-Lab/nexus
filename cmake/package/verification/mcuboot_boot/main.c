/**
 * \file            main.c
 * \brief           Unexecuted full bootutil link reference, without handoff.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "bootutil/bootutil.h"
#include "flash_map_backend/flash_map_backend.h"
#include "mcuboot_config/mcuboot_config.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "reference_regions.h"

static nx_flash_region_t primary_region;
static nx_flash_region_t secondary_region;
static const struct flash_area primary = {NX_MCUBOOT_PRIMARY_AREA_ID,
                                          &primary_region};
static const struct flash_area secondary = {NX_MCUBOOT_SECONDARY_AREA_ID,
                                            &secondary_region};
static nx_mcuboot_flash_operation_t operation;

static nx_time_us_t read_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}

const nx_mcuboot_flash_operation_t* nx_mcuboot_flash_operation(void) {
    return &operation;
}

void nx_mcuboot_assert_fail(void) {
    for (;;) {
    }
}

void nx_mcuboot_watchdog_progress(void) {
    /* No watchdog is selected in this unexecuted link-only fixture. */
}

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    const nx_flash_port_t* flash = nx_factory_flash(NX_FLASH_ID_PHYSICAL);
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(flash);
    if (geometry == NULL || REFERENCE_PRIMARY_ORIGIN < geometry->base_address ||
        REFERENCE_SECONDARY_ORIGIN < geometry->base_address) {
        return 2;
    }
    primary_region = (nx_flash_region_t){
        flash, REFERENCE_PRIMARY_ORIGIN - geometry->base_address,
        REFERENCE_PRIMARY_SIZE, true};
    secondary_region = (nx_flash_region_t){
        flash, REFERENCE_SECONDARY_ORIGIN - geometry->base_address,
        REFERENCE_SECONDARY_SIZE, true};
    operation = (nx_mcuboot_flash_operation_t){
        &primary,
        &secondary,
        {read_clock, NULL},
        nx_deadline_after(read_clock(NULL), 5000000U)};
    struct boot_rsp response = {0};
    fih_ret result = boot_go(&response);
    operation.primary = NULL;
    operation.secondary = NULL;
    nx_result_t stopped = nx_platform_stop();
    /* This fixture links normal selection; it never implements an MCU jump. */
    return FIH_EQ(result, FIH_SUCCESS) && stopped == NX_SUCCESS ? 0 : 3;
}

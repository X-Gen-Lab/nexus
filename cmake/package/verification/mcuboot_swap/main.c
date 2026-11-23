/**
 * \file            main.c
 * \brief           Exact F407ZG scratch boot-storage link reference, no jump
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "bootutil/bootutil.h"
#include "nexus/mcuboot_swap_port.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "reference_regions.h"

static nx_flash_region_t primary_region;
static nx_flash_region_t secondary_region;
static nx_flash_region_t scratch_region;
static const struct flash_area primary = {NX_MCUBOOT_PRIMARY_AREA_ID,
                                          &primary_region};
static const struct flash_area secondary = {NX_MCUBOOT_SECONDARY_AREA_ID,
                                            &secondary_region};
static const struct flash_area scratch = {NX_MCUBOOT_SCRATCH_AREA_ID,
                                          &scratch_region};
static nx_mcuboot_swap_flash_operation_t operation;

static nx_time_us_t read_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}

const nx_mcuboot_swap_flash_operation_t* nx_mcuboot_swap_flash_operation(void) {
    return &operation;
}

void nx_mcuboot_assert_fail(void) {
    for (;;) {
    }
}

void nx_mcuboot_watchdog_progress(void) {
    /* No watchdog is selected in this unexecuted link-only reference. */
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0) {
        return 1;
    }
    const nx_flash_port_t* flash = nx_factory_flash_ref(NX_FLASH_REF_PHYSICAL);
    const nx_flash_geometry_t* geometry = nx_flash_port_geometry(flash);
    if (geometry == NULL || REFERENCE_PRIMARY_ORIGIN < geometry->base_address ||
        REFERENCE_SECONDARY_ORIGIN < geometry->base_address ||
        REFERENCE_SCRATCH_ORIGIN < geometry->base_address) {
        return 2;
    }
    primary_region = (nx_flash_region_t){
        flash, REFERENCE_PRIMARY_ORIGIN - geometry->base_address,
        REFERENCE_PRIMARY_SIZE, true};
    secondary_region = (nx_flash_region_t){
        flash, REFERENCE_SECONDARY_ORIGIN - geometry->base_address,
        REFERENCE_SECONDARY_SIZE, true};
    scratch_region = (nx_flash_region_t){
        flash, REFERENCE_SCRATCH_ORIGIN - geometry->base_address,
        REFERENCE_SCRATCH_SIZE, true};
    operation = (nx_mcuboot_swap_flash_operation_t){
        &primary,
        &secondary,
        &scratch,
        {read_clock, NULL},
        nx_deadline_after(read_clock(NULL), 5000000)};
    struct boot_rsp response = {0};
    const fih_ret result = boot_go(&response);
    operation.primary = NULL;
    operation.secondary = NULL;
    operation.scratch = NULL;
    const nx_result_t stopped = nx_platform_stop();
    /* Link/selection only: actual image handoff belongs to the product. */
    return FIH_EQ(result, FIH_SUCCESS) && stopped == NX_SUCCESS ? 0 : 3;
}

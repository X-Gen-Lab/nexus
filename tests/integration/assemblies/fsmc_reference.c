/**
 * \file            fsmc_reference.c
 * \brief           C11 link consumer of explicitly selected FSMC raw faces
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/flash_async.h"
#include "nexus/io/memory_window.h"
#include "nexus/io/nand.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

static volatile bool execute;
static volatile nx_result_t observation;
#if defined(NX_REFERENCE_NOR)
static nx_flash_async_request_t nor_request;
static uint16_t program_word = UINT16_C(0x1234);
#endif
#if defined(NX_REFERENCE_NAND)
static nx_nand_request_t nand_request;
static uint8_t page[2048];
static uint8_t oob[64];
static uint32_t ecc[4];
#endif
#if defined(NX_REFERENCE_PSRAM)
static nx_memory_window_lease_t lease;
#endif
/** \brief Retain actual selected APIs for ARM linking, never execute as HIL. */
int main(void) {
    for (;;) {
        if (!execute)
            continue;
        execute = false;
        (void)nx_platform_start();
#if defined(NX_REFERENCE_NOR)
        const nx_flash_async_port_t* async =
            nx_factory_flash_async_ref(NX_FLASH_ASYNC_REF_NOR0);
        const nx_flash_port_t* flash =
            nx_factory_flash_async_flash_ref(NX_FLASH_ASYNC_REF_NOR0);
        const nx_flash_geometry_t* geometry = nx_flash_port_geometry(flash);
        nx_request_initialize(&nor_request.base);
        observation = nx_flash_async_prepare(
            &nor_request, NX_FLASH_ASYNC_PROGRAM, 0U, &program_word,
            geometry == NULL ? 2U : geometry->program_unit, 100U);
        observation = nx_flash_async_submit(async, &nor_request);
        observation = nx_flash_async_cancel(async, &nor_request);
        observation = nx_flash_async_service(async);
        observation = nx_flash_async_stop(async);
        observation = nx_flash_port_read(flash, 0U, &program_word, 2U);
#endif
#if defined(NX_REFERENCE_NAND)
        const nx_nand_port_t* raw = nx_factory_nand(NX_NAND_ID_NAND0);
        nx_nand_status_t status;
        observation = nx_nand_status(raw, &status);
        nx_nand_ecc_format_t format;
        observation = nx_nand_ecc_format(raw, &format);
        observation = nx_nand_id(raw, page, 5U);
        nx_request_initialize(&nand_request.base);
        observation = nx_nand_prepare(&nand_request, 100U);
        nand_request.operation = NX_NAND_READ_PAGE;
        nand_request.data = page;
        nand_request.bytes = sizeof(page);
        nand_request.oob = oob;
        nand_request.oob_bytes = sizeof(oob);
        nand_request.ecc = ecc;
        nand_request.ecc_count = 4U;
        observation = nx_nand_submit(raw, &nand_request);
        observation = nx_nand_cancel(raw, &nand_request);
        observation = nx_nand_service(raw);
        observation = nx_nand_stop(raw);
        if (nx_nand_geometry(raw) == NULL)
            observation = NX_ERROR_STATE;
#endif
#if defined(NX_REFERENCE_PSRAM)
        const nx_memory_window_port_t* window =
            nx_factory_memory_window(NX_MEMORY_WINDOW_ID_PSRAM0);
        nx_memory_window_geometry_t window_geometry;
        observation = nx_memory_window_port_geometry(window, &window_geometry);
        observation = nx_memory_window_port_acquire(window, &lease);
        observation =
            nx_memory_window_port_release(window, &lease, lease.epoch);
#endif
        observation = nx_platform_stop();
    }
}

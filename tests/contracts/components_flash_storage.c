/**
 * \file            components_flash_storage.c
 * \brief           Actual Flash-region adapter geometry and partial-write tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/flash_storage.h"
#include "nexus/io/native/model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            abort();                                                           \
        }                                                                      \
    } while (0)

/**
 * \brief           Exercise actual IO region boundaries without physical Flash
 *                  claims.
 */
int main(void) {
    uint8_t memory[512];
    const nx_flash_sector_t sectors[] = {{0, 128}, {128, 128}, {256, 256}};
    const nx_flash_geometry_t geometry = {0, 512, 4, sectors, 3};
    CHECK(nx_native_clock_configure(true, 0) == NX_SUCCESS);
    CHECK(nx_native_flash_configure(memory, &geometry) == NX_SUCCESS);
    nx_flash_storage_adapter_t adapter;
    nx_storage_port_t port = {0};
    nx_flash_region_t region = {nx_native_flash, 0, 512, true};
    CHECK(nx_flash_storage_bind(&adapter, region, 100000, &port) ==
          NX_ERROR_UNSUPPORTED);
    region.size = 256;
    region.writable = false;
    CHECK(nx_flash_storage_bind(&adapter, region, 100000, &port) ==
          NX_ERROR_PERMISSION);
    region.writable = true;
    CHECK(nx_flash_storage_bind(&adapter, region, 100000, &port) == NX_SUCCESS);
    CHECK(port.size == 256 && port.erase_size == 128 && port.program_size == 4);
    uint8_t workspace[32];
    nx_storage_t storage;
    CHECK(nx_storage_open(&storage, &port, 0, 128, workspace,
                          sizeof(workspace)) == NX_STORAGE_OK);
    CHECK(nx_storage_save(&storage, "old", 4) == NX_STORAGE_OK);
    nx_native_flash_fault(2);
    CHECK(nx_storage_save(&storage, "new", 4) == NX_STORAGE_IO);
    CHECK(!storage.opened);
    nx_native_flash_fault(SIZE_MAX);
    CHECK(nx_storage_open(&storage, &port, 0, 128, workspace,
                          sizeof(workspace)) == NX_STORAGE_OK);
    char bytes[16];
    size_t size = sizeof(bytes);
    CHECK(nx_storage_load(&storage, bytes, &size) == NX_STORAGE_OK);
    CHECK(size == 4 && memcmp(bytes, "old", 4) == 0);
    CHECK(nx_storage_save(&storage, workspace, 4) == NX_STORAGE_INVALID);
    nx_flash_storage_set_deadline(&adapter, nx_time_now_us());
    CHECK(nx_storage_save(&storage, "late", 5) == NX_STORAGE_IO);
    CHECK(!storage.opened);
    CHECK(port.read(port.ctx, UINT32_MAX, bytes, 1) == NX_STORAGE_INVALID);
    CHECK(memory[256] == 0xff && memory[511] == 0xff);
    puts("Flash adapter uniform geometry, bounds, shared deadline and recovery "
         "passed");
    return 0;
}

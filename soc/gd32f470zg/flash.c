#include "flash.h"
#include "arch/nx_arch.h"
#include "gd32f4xx.h"
#include <string.h>

#define STORAGE_BASE 0x080FC000u
#define STORAGE_SIZE 16384u
/* GD32F470 additional page erase, UM Rev3.3 2.3.4. Not sector geometry. */
#define PAGE_SIZE 4096u
extern unsigned char __nexus_storage_start[];
extern unsigned char __nexus_storage_end[];

static bool valid_range(size_t offset, size_t length) {
    return offset <= STORAGE_SIZE && length <= STORAGE_SIZE - offset;
}
static nx_storage_status_t flash_read(void* context, size_t offset,
                                      void* data, size_t length) {
    (void)context;
    if (nx_arch_in_isr() || !valid_range(offset, length) || (!data && length)) {
        return NX_STORAGE_INVALID;
    }
    if (length) { memcpy(data, (const void*)(STORAGE_BASE + offset), length); }
    return NX_STORAGE_OK;
}
static nx_storage_status_t flash_program(void* context, size_t offset,
                                         const void* data, size_t length) {
    (void)context;
    if (nx_arch_in_isr() || !valid_range(offset, length) || (!data && length) ||
        (offset % 2u) || (length % 2u)) { return NX_STORAGE_INVALID; }
    const unsigned char* bytes = data;
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
                   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);
    for (size_t i = 0; i < length; i += 2u) {
        uint16_t value;
        memcpy(&value, bytes + i, sizeof(value));
        uint32_t address = STORAGE_BASE + (uint32_t)(offset + i);
        uint16_t previous = *(volatile const uint16_t*)address;
        if ((previous & value) != value ||
            fmc_halfword_program(address, value) != FMC_READY ||
            *(volatile const uint16_t*)address != value) {
            fmc_lock();
            return NX_STORAGE_IO;
        }
    }
    fmc_lock();
    nx_arch_dsb();
    return NX_STORAGE_OK;
}
static nx_storage_status_t flash_erase(void* context, size_t offset, size_t length) {
    (void)context;
    if (nx_arch_in_isr() || !valid_range(offset, length) ||
        (offset % PAGE_SIZE) || (length % PAGE_SIZE)) { return NX_STORAGE_INVALID; }
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
                   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);
    for (size_t i = 0; i < length; i += PAGE_SIZE) {
        uint32_t address = STORAGE_BASE + (uint32_t)(offset + i);
        if (fmc_page_erase(address) != FMC_READY) {
            fmc_lock();
            return NX_STORAGE_IO;
        }
        for (size_t j = 0; j < PAGE_SIZE; j += sizeof(uint32_t)) {
            if (*(volatile const uint32_t*)(address + j) != UINT32_MAX) {
                fmc_lock();
                return NX_STORAGE_IO;
            }
        }
    }
    fmc_lock();
    nx_arch_dsb();
    return NX_STORAGE_OK;
}
static nx_storage_status_t flash_sync(void* context) {
    (void)context;
    if (nx_arch_in_isr()) { return NX_STORAGE_INVALID; }
    nx_arch_dsb();
    return fmc_state_get() == FMC_READY ? NX_STORAGE_OK : NX_STORAGE_IO;
}
const nx_flash_port_t* nx_gd32f470_flash_port(void) {
    if ((uintptr_t)__nexus_storage_start != STORAGE_BASE ||
        (uintptr_t)__nexus_storage_end != STORAGE_BASE + STORAGE_SIZE ||
        (*(volatile const uint32_t*)0x1FFF7A20u >> 16) != 1024u) { return NULL; }
    static const nx_flash_port_t port = {
        .size = STORAGE_SIZE, .erase_size = PAGE_SIZE, .program_size = 2,
        .read = flash_read, .program = flash_program, .erase = flash_erase,
        .sync = flash_sync
    };
    return &port;
}

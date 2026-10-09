/** Real full-chip provider with fault-capable vendor/register probes, no HIL. */
#define _GNU_SOURCE
#include "flash.h"
#include "hal/provider/nx_device_provider.h"
#include "hal/base/nx_device.h"
#include "arch/nx_arch.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

TestFlash fake_flash;
uint32_t SystemCoreClock=1000000;
static bool locked=true,fail_unlock,fail_lock,fail_program,fail_erase,skip_erase,hardware_timeout;
static uint32_t mask,basepri,faultmask,ipsr,unlocks,programs,erases,cache_resets,cycles,pulse_cycles=500,busy_probes;
static nx_flash_operations_t* operations;
static const uint32_t sector_offsets[]={0,0x4000,0x8000,0xC000,0x10000,0x20000,
    0x40000,0x60000,0x80000,0xA0000,0xC0000,0xE0000,0x100000};
nx_arch_irq_state_t nx_arch_irq_save(void) { nx_arch_irq_state_t old={mask}; mask=1; return old; }
void nx_arch_irq_restore(nx_arch_irq_state_t old) { mask=old.value; }
bool nx_arch_in_isr(void) { return ipsr!=0; }
bool nx_arch_irq_is_masked(void) { return mask || basepri || faultmask; }
void nx_arch_dmb(void) { }
void nx_arch_dsb(void) { }
void nx_arch_isb(void) { }
uint32_t stm32_perf_get_cycles(void) { return ++cycles; }
void fake_flash_cache_reset(void) { ++cache_resets; }
void fake_flash_clear_flags(uint32_t flags) { assert(flags); }
int fake_flash_busy(void) { if (busy_probes) { --busy_probes; return 1; } return 0; }
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { ++unlocks; if (fail_unlock) return HAL_ERROR; locked=false; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { if (fail_lock) return HAL_ERROR; locked=true; return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t value) {
    assert(!locked && type==FLASH_TYPEPROGRAM_WORD && address>=0x08000000U && address+4U<=0x08000000U+NX_CONFIG_STM32_FLASH_SIZE);
    ++programs;
    uint32_t scratch;
    assert(operations->read(operations,0,(uint8_t*)&scratch,4)==NX_ERR_BUSY);
    cycles+=pulse_cycles;
    if (hardware_timeout) { busy_probes=3; return HAL_TIMEOUT; }
    if (fail_program) return HAL_ERROR;
    *(uint32_t*)(uintptr_t)address&=(uint32_t)value;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* erase,uint32_t* failed) {
    assert(!locked && erase->TypeErase==FLASH_TYPEERASE_SECTORS && erase->VoltageRange==FLASH_VOLTAGE_RANGE_3);
    assert(erase->NbSectors==1 && erase->Sector<(NX_CONFIG_STM32_FLASH_SIZE==0x80000 ? 8U : 12U));
    ++erases; cycles+=pulse_cycles;
    if (fail_erase) { *failed=erase->Sector; return HAL_ERROR; }
    if (!skip_erase) memset((void*)(uintptr_t)(0x08000000U+sector_offsets[erase->Sector]),0xFF,
        sector_offsets[erase->Sector+1]-sector_offsets[erase->Sector]);
    return HAL_OK;
}
static void map_memory(uintptr_t address,size_t length) {
    assert(mmap((void*)address,length,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)address);
}
int main(void) {
    map_memory(0x08000000U,1048576); map_memory(0x1FFF7000U,4096);
    memset((void*)0x08000000U,0xFF,1048576);
    *(uint16_t*)(uintptr_t)FLASHSIZE_BASE=NX_CONFIG_STM32_FLASH_SIZE/1024U;
    fake_flash.ACR=FLASH_ACR_DCEN|FLASH_ACR_ICEN;
    extern const nx_device_t STM32_INTERNAL_FLASH0;
    assert(STM32_INTERNAL_FLASH0.device_class==NX_DEVICE_CLASS_FLASH);
    assert(STM32_INTERNAL_FLASH0.capabilities&NX_DEVICE_CAP_FLASH_GEOMETRY);
    void* api=NULL;
    assert(STM32_INTERNAL_FLASH0.construct(&STM32_INTERNAL_FLASH0,&api)==NX_OK && api);
    nx_internal_flash_t* flash=api;
    nx_lifecycle_t* lifecycle=flash->get_lifecycle(flash);
    operations=flash->get_operations(flash);
    nx_flash_geometry_t geometry;
    assert(operations->get_geometry(operations,&geometry)==NX_ERR_NOT_INIT);
#if defined(NEXUS_TEST_MISMATCH_DENSITY)
    /* Old fixed layout requirement is gone; only physical density must match. */
    *(uint16_t*)(uintptr_t)FLASHSIZE_BASE=NX_CONFIG_STM32_FLASH_SIZE==0x80000 ? 1024 : 512;
    assert(lifecycle->init(lifecycle)==NX_ERR_INVALID_STATE);
#else
    assert(lifecycle->init(lifecycle)==NX_OK && locked);
    assert(operations->get_geometry(operations,&geometry)==NX_OK);
    assert(geometry.size_bytes==NX_CONFIG_STM32_FLASH_SIZE && geometry.base_address==0x08000000U);
    assert(geometry.block_count==(NX_CONFIG_STM32_FLASH_SIZE==0x80000 ? 8U : 12U));
    nx_flash_block_t block;
    assert(operations->get_block(operations,0,&block)==NX_OK && block.size==16384);
    assert(operations->get_block(operations,0x10000,&block)==NX_OK && block.size==65536);
    assert(operations->get_block(operations,geometry.size_bytes-1,&block)==NX_OK && block.size==131072);
    uint32_t word=0x12345678,readback=0;
    assert(operations->program(operations,0,(uint8_t*)&word,4,100)==NX_ERR_INVALID_STATE && !programs);
    fail_unlock=true; assert(flash->unlock(flash)==NX_ERR_IO && locked); fail_unlock=false;
    assert(flash->unlock(flash)==NX_OK && !locked);
    uint32_t* masks[]={&mask,&basepri,&faultmask};
    for (unsigned i=0;i<3;++i) {
        *masks[i]=1;
        assert(operations->program(operations,0,(uint8_t*)&word,4,100)==NX_ERR_INVALID_STATE && *masks[i]==1);
        assert(operations->erase(operations,0,16384,100)==NX_ERR_INVALID_STATE);
        assert(operations->read(operations,0,(uint8_t*)&readback,4)==NX_OK && readback==UINT32_MAX);
        assert(!programs && !erases); *masks[i]=0;
    }
    assert(operations->program(operations,0,(uint8_t*)&word,4,100)==NX_OK && !locked);
    assert(operations->read(operations,0,(uint8_t*)&readback,4)==NX_OK && readback==word);
    uint32_t ones=UINT32_MAX;
    assert(operations->program(operations,0,(uint8_t*)&ones,4,100)==NX_ERR_INVALID_STATE);
    assert(operations->read(operations,geometry.size_bytes,NULL,0)==NX_OK);
    assert(operations->read(operations,geometry.size_bytes,(uint8_t*)&word,4)==NX_ERR_INVALID_PARAM);
    assert(operations->read(operations,UINT32_MAX,(uint8_t*)&word,4)==NX_ERR_INVALID_PARAM);
    assert(operations->read(operations,1,(uint8_t*)&word,SIZE_MAX)==NX_ERR_INVALID_PARAM);
    assert(operations->program(operations,1,(uint8_t*)&word,4,100)==NX_ERR_INVALID_PARAM);
    assert(operations->program(operations,0,(uint8_t*)&word,3,100)==NX_ERR_INVALID_PARAM);
    assert(operations->erase(operations,0,1,100)==NX_ERR_INVALID_PARAM);
    assert(operations->erase(operations,0,81920,100)==NX_ERR_INVALID_PARAM);
    assert(operations->erase(operations,geometry.size_bytes,16384,100)==NX_ERR_INVALID_PARAM);
    ipsr=16;
    assert(operations->read(operations,0,(uint8_t*)&word,4)==NX_ERR_INVALID_STATE);
    assert(operations->sync(operations,100)==NX_ERR_INVALID_STATE); ipsr=0;
    fail_program=true; assert(operations->program(operations,4,(uint8_t*)&word,4,100)==NX_ERR_IO); fail_program=false;
    fail_erase=true; assert(operations->erase(operations,0,16384,100)==NX_ERR_IO); fail_erase=false;
    skip_erase=true; assert(operations->erase(operations,0,16384,100)==NX_ERR_IO); skip_erase=false;
    uint32_t count=programs;
    assert(operations->program(operations,4,(uint8_t*)&word,4,0)==NX_ERR_TIMEOUT && programs==count);
    pulse_cycles=2000;
    uint32_t pair[]={word,word};
    assert(operations->program(operations,4,(uint8_t*)pair,8,1)==NX_ERR_TIMEOUT && programs==count+1);
    assert(*(uint32_t*)0x08000004==word && *(uint32_t*)0x08000008==UINT32_MAX);
    pulse_cycles=500; cycles=UINT32_MAX-100;
    assert(operations->program(operations,8,(uint8_t*)&word,4,100)==NX_OK);
    hardware_timeout=true;
    assert(operations->program(operations,12,(uint8_t*)&word,4,100)==NX_ERR_TIMEOUT && busy_probes==0); hardware_timeout=false;
    count=erases;
    assert(operations->erase(operations,0,geometry.size_bytes,100)==NX_OK);
    assert(erases-count==geometry.block_count && cache_resets);
    assert((fake_flash.ACR&(FLASH_ACR_DCEN|FLASH_ACR_ICEN))==(FLASH_ACR_DCEN|FLASH_ACR_ICEN));
    assert(operations->program(operations,geometry.size_bytes-4,(uint8_t*)&word,4,100)==NX_OK);
    assert(operations->erase(operations,block.offset,block.size,100)==NX_OK);
    assert(operations->read(operations,geometry.size_bytes-4,(uint8_t*)&readback,4)==NX_OK && readback==UINT32_MAX);
    *(uint16_t*)(uintptr_t)FLASHSIZE_BASE=NX_CONFIG_STM32_FLASH_SIZE==0x80000 ? 1024 : 512;
    assert(operations->get_geometry(operations,&geometry)==NX_ERR_INVALID_STATE);
    *(uint16_t*)(uintptr_t)FLASHSIZE_BASE=NX_CONFIG_STM32_FLASH_SIZE/1024U;
    fail_lock=true; assert(lifecycle->deinit(lifecycle)==NX_ERR_IO); fail_lock=false;
    assert(lifecycle->get_state(lifecycle)==NX_DEV_STATE_RUNNING);
    assert(lifecycle->deinit(lifecycle)==NX_OK && locked);
    assert(operations->read(operations,0,(uint8_t*)&word,4)==NX_ERR_NOT_INIT);
    assert(lifecycle->init(lifecycle)==NX_OK && lifecycle->deinit(lifecycle)==NX_OK);
#endif
    puts("STM32 whole-chip provider, exact sectors, explicit protection, deadlines and settlement passed");
    return 0;
}

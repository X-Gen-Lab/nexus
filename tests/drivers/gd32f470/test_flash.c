/** Real full F470 Flash provider with fault probes; no physical HIL. */
#define _GNU_SOURCE
#include "flash.h"
#include "hal/provider/nx_device_provider.h"
#include "identity.h"
#include "hal/base/nx_device.h"
#include "gd32f4xx.h"
#include "arch/nx_arch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
uint32_t fake_fmc_ctl=FMC_CTL_LK;
static bool fail_unlock,fail_lock,fail_program,fail_erase,skip_erase,hardware_timeout;
static uint32_t programs,erases,ipsr,mask,milliseconds,busy_probes;
static nx_flash_operations_t* operations;
uint32_t nx_gd32f470_millis(void) { return milliseconds; }
nx_arch_irq_state_t nx_arch_irq_save(void) { nx_arch_irq_state_t old={mask}; mask=1; return old; }
void nx_arch_irq_restore(nx_arch_irq_state_t old) { mask=old.value; }
bool nx_arch_in_isr(void) { return ipsr!=0; }
bool nx_arch_irq_is_masked(void) { return mask!=0; }
void nx_arch_dmb(void) { }
void nx_arch_dsb(void) { }
void nx_arch_isb(void) { }
void fmc_unlock(void) { if (!fail_unlock) fake_fmc_ctl&=~FMC_CTL_LK; }
void fmc_lock(void) { if (!fail_lock) fake_fmc_ctl|=FMC_CTL_LK; }
void fmc_flag_clear(uint32_t flags) { assert(flags); }
fmc_state_enum fmc_state_get(void) { if (busy_probes) { --busy_probes; return FMC_BUSY; } return FMC_READY; }
fmc_state_enum fmc_halfword_program(uint32_t address,uint16_t value) {
    assert(!(fake_fmc_ctl&FMC_CTL_LK) && address>=0x08000000U && address+2U<=0x08100000U);
    ++programs; milliseconds+=5;
    uint16_t scratch;
    assert(operations->read(operations,0,(uint8_t*)&scratch,2)==NX_ERR_BUSY);
    if (hardware_timeout) { busy_probes=4; return FMC_TOERR; }
    if (fail_program) return FMC_OPERR;
    *(uint16_t*)(uintptr_t)address&=value;
    return FMC_READY;
}
fmc_state_enum fmc_page_erase(uint32_t address) {
    assert(!(fake_fmc_ctl&FMC_CTL_LK) && address>=0x08000000U && address+4096U<=0x08100000U && address%4096U==0);
    ++erases; milliseconds+=5;
    if (fail_erase) return FMC_OPERR;
    if (!skip_erase) memset((void*)(uintptr_t)address,0xFF,4096);
    return FMC_READY;
}
static void map_memory(uintptr_t address,size_t size) {
    assert(mmap((void*)address,size,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)address);
}
int main(void) {
    map_memory(0x08000000U,1048576); map_memory(0x1FFF7000U,4096);
    memset((void*)0x08000000U,0xA5,1048576);
    *(uint32_t*)0x1FFF7A20=(1024U<<16)|512U;
    uint32_t* uid=(uint32_t*)0x1FFF7A10;
    uid[0]=0x11111111; uid[1]=0x22222222; uid[2]=0x33333333;
    nx_gd32f470_identity_t identity;
    nx_gd32f470_identity(&identity);
    assert(identity.uid[0]==uid[0] && identity.flash_kib==1024 && identity.sram_kib==512);
    extern const nx_device_t GD32_INTERNAL_FLASH0;
    assert(GD32_INTERNAL_FLASH0.device_class==NX_DEVICE_CLASS_FLASH);
    void* api=NULL;
    assert(GD32_INTERNAL_FLASH0.construct(&GD32_INTERNAL_FLASH0,&api)==NX_OK && api);
    nx_internal_flash_t* flash=api;
    nx_lifecycle_t* lifecycle=flash->get_lifecycle(flash);
    operations=flash->get_operations(flash);
    nx_flash_geometry_t geometry;
    assert(operations->get_geometry(operations,&geometry)==NX_ERR_NOT_INIT);
    assert(lifecycle->init(lifecycle)==NX_OK && (fake_fmc_ctl&FMC_CTL_LK));
    assert(operations->get_geometry(operations,&geometry)==NX_OK);
    assert(geometry.size_bytes==1048576 && geometry.block_count==256 && geometry.program_alignment==2);
    nx_flash_block_t block;
    assert(operations->get_block(operations,geometry.size_bytes-1,&block)==NX_OK);
    assert(block.offset==1044480 && block.size==4096 && block.index==255);
    uint16_t value=0x1234,actual=0;
    assert(operations->erase(operations,0,4096,100)==NX_ERR_INVALID_STATE && !erases);
    fail_unlock=true; assert(flash->unlock(flash)==NX_ERR_IO); fail_unlock=false;
    assert(flash->unlock(flash)==NX_OK && !(fake_fmc_ctl&FMC_CTL_LK));
    /* Explicitly select four pages in this test, with untouched adjacent data. */
    assert(operations->erase(operations,0xFC000,16384,100)==NX_OK && erases==4);
    for (uintptr_t address=0x08000000;address<0x080FC000;++address)
        assert(*(uint8_t*)address==0xA5);
    assert(operations->program(operations,1048574,(uint8_t*)&value,2,100)==NX_OK);
    assert(operations->read(operations,1048574,(uint8_t*)&actual,2)==NX_OK && actual==value);
    uint32_t count=programs;
    value=UINT16_MAX;
    assert(operations->program(operations,1048574,(uint8_t*)&value,2,100)==NX_ERR_INVALID_STATE && programs==count);
    assert(operations->erase(operations,1,4096,100)==NX_ERR_INVALID_PARAM);
    assert(operations->erase(operations,0,2048,100)==NX_ERR_INVALID_PARAM);
    assert(operations->erase(operations,1048576,4096,100)==NX_ERR_INVALID_PARAM);
    assert(operations->read(operations,UINT32_MAX,(uint8_t*)&actual,2)==NX_ERR_INVALID_PARAM);
    assert(operations->read(operations,0,(uint8_t*)&actual,SIZE_MAX)==NX_ERR_INVALID_PARAM);
    assert(operations->program(operations,1048575,(uint8_t*)&actual,2,100)==NX_ERR_INVALID_PARAM);
    fail_erase=true; assert(operations->erase(operations,0,4096,100)==NX_ERR_IO); fail_erase=false;
    skip_erase=true; assert(operations->erase(operations,0,4096,100)==NX_ERR_IO); skip_erase=false;
    assert(operations->erase(operations,0,4096,100)==NX_OK);
    fail_program=true; assert(operations->program(operations,0,(uint8_t*)&actual,2,100)==NX_ERR_IO); fail_program=false;
    count=programs;
    assert(operations->program(operations,0,(uint8_t*)&actual,2,0)==NX_ERR_TIMEOUT && programs==count);
    uint16_t pair[]={actual,actual};
    assert(operations->program(operations,0,(uint8_t*)pair,4,1)==NX_ERR_TIMEOUT && programs==count+1);
    assert(*(uint16_t*)0x08000000==actual && *(uint16_t*)0x08000002==UINT16_MAX);
    milliseconds=UINT32_MAX-1;
    assert(operations->program(operations,2,(uint8_t*)&actual,2,100)==NX_OK);
    hardware_timeout=true;
    assert(operations->program(operations,4,(uint8_t*)&actual,2,100)==NX_ERR_TIMEOUT && busy_probes==0); hardware_timeout=false;
    mask=1;
    assert(operations->erase(operations,0,4096,100)==NX_ERR_INVALID_STATE && mask==1);
    assert(operations->read(operations,0,(uint8_t*)&actual,2)==NX_OK && mask==1); mask=0;
    ipsr=1;
    assert(operations->read(operations,0,(uint8_t*)&actual,2)==NX_ERR_INVALID_STATE);
    assert(operations->sync(operations,100)==NX_ERR_INVALID_STATE); ipsr=0;
    count=erases;
    assert(operations->erase(operations,0,geometry.size_bytes,10000)==NX_OK && erases-count==256);
    *(uint32_t*)0x1FFF7A20=(512U<<16)|256U;
    assert(operations->get_geometry(operations,&geometry)==NX_ERR_INVALID_STATE);
    *(uint32_t*)0x1FFF7A20=(1024U<<16)|512U;
    fail_lock=true; assert(lifecycle->deinit(lifecycle)==NX_ERR_IO); fail_lock=false;
    assert(lifecycle->get_state(lifecycle)==NX_DEV_STATE_RUNNING);
    assert(lifecycle->deinit(lifecycle)==NX_OK && (fake_fmc_ctl&FMC_CTL_LK));
    assert(operations->read(operations,0,(uint8_t*)&actual,2)==NX_ERR_NOT_INIT);
    assert(lifecycle->init(lifecycle)==NX_OK && lifecycle->deinit(lifecycle)==NX_OK);
    puts("GD32 full physical pages, density, explicit protection, budgets and settlement passed");
    return 0;
}

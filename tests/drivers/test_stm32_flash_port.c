#include "hal/provider/nx_device_provider.h"
/** Real STM32 Flash port, fixed virtual address memory and faulting HAL model.
 * Exercises runtime physical-size/linker fences. Not erase/power-fail HIL. */
#define _GNU_SOURCE
#include "flash.h"
#include "arch/nx_arch.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define STR_(x) #x
#define STR(x) STR_(x)
#if NX_CONFIG_STM32_FLASH_SIZE == 0x80000
#define EXPECTED_BASE 0x08040000
#define EXPECTED_END 0x08080000
#define EXPECTED_SECTOR 6
#else
#define EXPECTED_BASE 0x080C0000
#define EXPECTED_END 0x08100000
#define EXPECTED_SECTOR 10
#endif
#if defined(NEXUS_TEST_MISMATCH_LAYOUT)
__asm__(".global __nexus_storage_start\n.set __nexus_storage_start,0x08000000\n"
        ".global __nexus_storage_end\n.set __nexus_storage_end," STR(EXPECTED_END) "\n");
#else
__asm__(".global __nexus_storage_start\n.set __nexus_storage_start," STR(EXPECTED_BASE) "\n"
        ".global __nexus_storage_end\n.set __nexus_storage_end," STR(EXPECTED_END) "\n");
#endif
TestFlash fake_flash;
static bool locked=true,fail_unlock,fail_lock,fail_program,fail_erase,skip_erase;
static uint32_t mask,basepri,faultmask,ipsr,unlocks,programs,erases,last_sector,last_count,cache_resets;
static const nx_flash_port_t* port;
nx_arch_irq_state_t nx_arch_irq_save(void) {nx_arch_irq_state_t old={mask};mask=1;return old;}
void nx_arch_irq_restore(nx_arch_irq_state_t old) {mask=old.value;}
bool nx_arch_in_isr(void) {return ipsr!=0;}
bool nx_arch_irq_is_masked(void) {return mask || basepri || faultmask;}
void nx_arch_dmb(void) {}
void nx_arch_dsb(void) {}
void nx_arch_isb(void) {}
void fake_flash_cache_reset(void) {cache_resets++;}
void fake_flash_clear_flags(uint32_t flags) {assert(flags);}
HAL_StatusTypeDef HAL_FLASH_Unlock(void) {unlocks++;if(fail_unlock)return HAL_ERROR;locked=false;return HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Lock(void) {locked=true;return fail_lock ? HAL_ERROR : HAL_OK;}
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t value) {
    assert(!locked && type==FLASH_TYPEPROGRAM_WORD);
    assert(address>=EXPECTED_BASE && address+4U<=EXPECTED_END);
    programs++;
    /* An overlapping API cannot read while programming owns the controller. */
    uint32_t scratch; assert(port->read(port->ctx,0,&scratch,4)==NX_STORAGE_IO);
    if(fail_program)return HAL_ERROR;
    *(uint32_t*)(uintptr_t)address &= (uint32_t)value;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef* erase,uint32_t* failed) {
    assert(!locked && erase->TypeErase==FLASH_TYPEERASE_SECTORS);
    assert(erase->VoltageRange==FLASH_VOLTAGE_RANGE_3 && erase->NbSectors<=2);
    last_sector=erase->Sector;last_count=erase->NbSectors;erases++;
    if(fail_erase) {*failed=erase->Sector;return HAL_ERROR;}
    if(!skip_erase) {
        /* Independent ST sector model: sectors0..3 16K,4 64K,5..11 128K. */
        for(uint32_t sector=erase->Sector;sector<erase->Sector+erase->NbSectors;sector++) {
            assert(sector>=5 && sector<=11);
            uint32_t address=0x08020000U+(sector-5U)*128U*1024U;
            assert(address>=EXPECTED_BASE && address+128U*1024U<=EXPECTED_END);
            memset((void*)(uintptr_t)address,0xFF,128U*1024U);
        }
    }
    return HAL_OK;
}
static void map_memory(uintptr_t address,size_t length) {
    void* mapped=mmap((void*)address,length,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(mapped==(void*)address);
}
int main(void) {
    map_memory(0x08000000U,1024U*1024U);
    map_memory(0x1FFF7000U,4096U);
    memset((void*)0x08000000U,0xFF,1024U*1024U);
    *(uint16_t*)(uintptr_t)FLASHSIZE_BASE=NX_CONFIG_STM32_FLASH_SIZE/1024U;
    fake_flash.ACR=FLASH_ACR_DCEN;
#if defined(NEXUS_TEST_MISMATCH_LAYOUT)
    assert(nx_stm32f407_flash_port()==NULL);
#else
    port=nx_stm32f407_flash_port();assert(port && port->size==256U*1024U);
    assert(port->erase_size==128U*1024U && port->program_size==4);
    uint32_t word=0x12345678,readback=0;
    uint32_t* masks[]={&mask,&basepri,&faultmask};
    for(unsigned i=0;i<3;i++) {
        *masks[i]=1;
        assert(port->program(port->ctx,0,&word,4)==NX_STORAGE_INVALID && *masks[i]==1 && locked);
        assert(port->erase(port->ctx,0,port->erase_size)==NX_STORAGE_INVALID);
        assert(!unlocks && !programs && !erases);
        /* Read and cache sync do not depend on HAL time or IRQ completion. */
        assert(port->read(port->ctx,0,&readback,4)==NX_STORAGE_OK && readback==UINT32_MAX && *masks[i]==1);
        assert(port->sync(port->ctx)==NX_STORAGE_OK && *masks[i]==1);
        *masks[i]=0;
    }
    assert(port->program(port->ctx,0,&word,4)==NX_STORAGE_OK && locked);
    mask=1;
    assert(port->read(port->ctx,0,&readback,4)==NX_STORAGE_OK && readback==word && mask==1);
    mask=0;
    uint32_t ones=UINT32_MAX;
    assert(port->program(port->ctx,0,&ones,4)==NX_STORAGE_INVALID && locked);
    assert(port->read(port->ctx,port->size,NULL,0)==NX_STORAGE_OK);
    assert(port->read(port->ctx,port->size,&readback,4)==NX_STORAGE_INVALID);
    assert(port->read(port->ctx,SIZE_MAX,&readback,4)==NX_STORAGE_INVALID);
    assert(port->read(port->ctx,1,&readback,SIZE_MAX)==NX_STORAGE_INVALID);
    assert(port->program(port->ctx,1,&word,4)==NX_STORAGE_INVALID);
    assert(port->program(port->ctx,0,&word,3)==NX_STORAGE_INVALID);
    assert(port->erase(port->ctx,0,1)==NX_STORAGE_INVALID);
    assert(port->erase(port->ctx,port->size,port->erase_size)==NX_STORAGE_INVALID);
    ipsr=16;assert(port->read(port->ctx,0,&readback,4)==NX_STORAGE_INVALID);
    assert(port->program(port->ctx,4,&word,4)==NX_STORAGE_INVALID);
    assert(port->erase(port->ctx,0,port->erase_size)==NX_STORAGE_INVALID);
    assert(port->sync(port->ctx)==NX_STORAGE_INVALID);ipsr=0;
    fail_unlock=true;assert(port->program(port->ctx,4,&word,4)==NX_STORAGE_IO);fail_unlock=false;
    fail_program=true;assert(port->program(port->ctx,4,&word,4)==NX_STORAGE_IO && locked);fail_program=false;
    assert(port->read(port->ctx,4,&readback,4)==NX_STORAGE_OK && readback==UINT32_MAX);
    fail_lock=true;assert(port->program(port->ctx,8,&word,4)==NX_STORAGE_IO);fail_lock=false;
    fail_erase=true;assert(port->erase(port->ctx,0,port->erase_size)==NX_STORAGE_IO && locked);fail_erase=false;
    skip_erase=true;assert(port->erase(port->ctx,0,port->erase_size)==NX_STORAGE_IO);skip_erase=false;
    assert(port->erase(port->ctx,0,port->erase_size)==NX_STORAGE_OK && last_sector==EXPECTED_SECTOR && last_count==1);
    assert(port->program(port->ctx,port->size-4,&word,4)==NX_STORAGE_OK);
    assert(port->erase(port->ctx,port->erase_size,port->erase_size)==NX_STORAGE_OK && last_sector==EXPECTED_SECTOR+1 && last_count==1);
    assert(port->read(port->ctx,port->size-4,&readback,4)==NX_STORAGE_OK && readback==UINT32_MAX);
    assert(port->erase(port->ctx,0,port->size)==NX_STORAGE_OK && last_sector==EXPECTED_SECTOR && last_count==2);
    assert(port->sync(port->ctx)==NX_STORAGE_OK && cache_resets && (fake_flash.ACR&FLASH_ACR_DCEN));
    unsigned count=programs+erases;
    /* A compiled xG image must not expose storage on xE silicon and vice versa. */
    *(uint16_t*)(uintptr_t)FLASHSIZE_BASE=NX_CONFIG_STM32_FLASH_SIZE==0x80000 ? 1024 : 512;
    assert(nx_stm32f407_flash_port()==NULL);
    assert(port->read(port->ctx,0,&readback,4)==NX_STORAGE_INVALID);
    assert(port->program(port->ctx,0,&word,4)==NX_STORAGE_INVALID);
    assert(port->erase(port->ctx,0,port->erase_size)==NX_STORAGE_INVALID && programs+erases==count);
#endif
    puts("STM32 real Flash physical/linker fences, boundaries, controller faults and verification passed");
    return 0;
}

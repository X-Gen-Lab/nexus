/** Full STM32F407 physical Flash provider. Product regions are external. */
#include "hal/provider/nx_device_provider.h"
#include "flash.h"
#include "flash_geometry.h"
#include "hal/base/nx_device.h"
#include "arch/nx_arch.h"
#include "nexus_config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Already enabled/verified by platform boot. No reset on device open. */
extern uint32_t stm32_perf_get_cycles(void);
static struct {
    nx_device_config_state_t core;
    nx_internal_flash_t api;
    nx_flash_operations_t operations;
    nx_lifecycle_t lifecycle;
    bool initialized, unlocked, busy;
} device;

typedef struct {
    uint32_t previous, frequency, timeout_ms;
    uint64_t elapsed_cycles;
} flash_deadline_t;
static bool physical_geometry(nx_flash_geometry_t* geometry) {
    uint32_t bytes=(uint32_t)*(volatile const uint16_t*)FLASHSIZE_BASE*1024U;
    return bytes==NX_CONFIG_STM32_FLASH_SIZE && nx_stm32f407_flash_geometry(bytes,geometry);
}
static nx_status_t take(bool writing) {
    if (nx_arch_in_isr() || (writing && nx_arch_irq_is_masked())) return NX_ERR_INVALID_STATE;
    nx_arch_irq_state_t key=nx_arch_irq_save();
    nx_status_t status=!device.initialized ? NX_ERR_NOT_INIT : device.busy ? NX_ERR_BUSY : NX_OK;
    if (status==NX_OK) device.busy=true;
    nx_arch_dmb(); nx_arch_irq_restore(key);
    return status;
}
static void give(void) {
    nx_arch_irq_state_t key=nx_arch_irq_save();
    device.busy=false; nx_arch_dmb(); nx_arch_irq_restore(key);
}
static nx_status_t begin(nx_flash_operations_t* self,nx_flash_geometry_t* geometry,bool writing) {
    if (self!=&device.operations) return NX_ERR_INVALID_PARAM;
    nx_status_t status=take(writing);
    if (status!=NX_OK) return status;
    if (!physical_geometry(geometry)) { give(); return NX_ERR_INVALID_STATE; }
    if (writing && !device.unlocked) { give(); return NX_ERR_INVALID_STATE; }
    return NX_OK;
}
static void flush_cache(void) {
    bool data_enabled=(FLASH->ACR&FLASH_ACR_DCEN)!=0;
    bool instruction_enabled=(FLASH->ACR&FLASH_ACR_ICEN)!=0;
    __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_RESET();
    __HAL_FLASH_DATA_CACHE_RESET();
    if (instruction_enabled) __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    if (data_enabled) __HAL_FLASH_DATA_CACHE_ENABLE();
    nx_arch_dsb(); nx_arch_isb();
}
/* CMSIS DWT advances while same-bank fetch stalls; HAL SysTick may miss ticks.
 * Sample after every hardware pulse. One pulse must stay below one DWT wrap;
 * total multi-sector elapsed time is accumulated in 64 bits. Debug halt or
 * SYSCLK changes during maintenance are unsupported. Real timing needs HIL. */
static nx_status_t deadline_start(flash_deadline_t* deadline,uint32_t timeout) {
    if (timeout!=UINT32_MAX && timeout>=0x80000000U) return NX_ERR_INVALID_PARAM;
    uint32_t first=stm32_perf_get_cycles();
    uint32_t next=stm32_perf_get_cycles();
    if (!SystemCoreClock || next==first) return NX_ERR_NOT_SUPPORTED;
    *deadline=(flash_deadline_t){.previous=next,.frequency=SystemCoreClock,.timeout_ms=timeout};
    return NX_OK;
}
static bool expired(flash_deadline_t* deadline) {
    uint32_t current=stm32_perf_get_cycles();
    deadline->elapsed_cycles+=(uint32_t)(current-deadline->previous);
    deadline->previous=current;
    return deadline->timeout_ms!=UINT32_MAX &&
        deadline->elapsed_cycles*1000U>=(uint64_t)deadline->timeout_ms*deadline->frequency;
}
static void settle(void) {
    /* A timed-out SDK call must not leave a pulse owned by a returned caller.
     * If silicon never settles, an external watchdog/reset policy is required. */
    while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY)) { }
    nx_arch_dsb();
}
static nx_status_t geometry_query(nx_flash_operations_t* self,nx_flash_geometry_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,false);
    if (status==NX_OK) { *out=geometry; give(); }
    return status;
}
static nx_status_t block_query(nx_flash_operations_t* self,uint32_t offset,nx_flash_block_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    nx_flash_geometry_t geometry;
    nx_status_t status=geometry_query(self,&geometry);
    return status!=NX_OK ? status : nx_stm32f407_flash_block(&geometry,offset,out) ? NX_OK : NX_ERR_INVALID_PARAM;
}
static nx_status_t read_data(nx_flash_operations_t* self,uint32_t offset,uint8_t* data,size_t length) {
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,false);
    if (status!=NX_OK) return status;
    if (!nx_stm32f407_flash_range(&geometry,offset,length) || (!data && length)) status=NX_ERR_INVALID_PARAM;
    else if (length) memcpy(data,(const void*)(geometry.base_address+offset),length);
    give(); return status;
}
static nx_status_t program_data(nx_flash_operations_t* self,uint32_t offset,
                                const uint8_t* data,size_t length,uint32_t timeout) {
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,true);
    if (status!=NX_OK) return status;
    if (!nx_stm32f407_flash_range(&geometry,offset,length) || (!data && length) || offset%4U || length%4U) {
        give(); return NX_ERR_INVALID_PARAM;
    }
    if (!length) { give(); return NX_OK; }
    flash_deadline_t deadline;
    status=deadline_start(&deadline,timeout);
    if (status!=NX_OK) { give(); return status; }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
                            FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
    for (size_t i=0;i<length;i+=4U) {
        if (expired(&deadline)) { status=NX_ERR_TIMEOUT; break; }
        uint32_t word; memcpy(&word,data+i,sizeof(word));
        uint32_t address=(uint32_t)geometry.base_address+offset+(uint32_t)i;
        if ((*(volatile const uint32_t*)(uintptr_t)address&word)!=word) { status=NX_ERR_INVALID_STATE; break; }
        HAL_StatusTypeDef hardware=HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,address,word);
        settle(); flush_cache();
        if (hardware!=HAL_OK) { status=hardware==HAL_TIMEOUT ? NX_ERR_TIMEOUT : NX_ERR_IO; break; }
        if (*(volatile const uint32_t*)(uintptr_t)address!=word) { status=NX_ERR_IO; break; }
        if (SystemCoreClock!=deadline.frequency) { status=NX_ERR_INVALID_STATE; break; }
        if (expired(&deadline)) { status=NX_ERR_TIMEOUT; break; }
    }
    give(); return status;
}
static nx_status_t erase_data(nx_flash_operations_t* self,uint32_t offset,size_t length,uint32_t timeout) {
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,true);
    if (status!=NX_OK) return status;
    uint32_t first,count;
    if (!nx_stm32f407_flash_erase_sector(&geometry,offset,length,&first,&count)) { give(); return NX_ERR_INVALID_PARAM; }
    if (!length) { give(); return NX_OK; }
    flash_deadline_t deadline;
    status=deadline_start(&deadline,timeout);
    if (status!=NX_OK) { give(); return status; }
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|
                            FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
    uint32_t current_offset=offset;
    for (uint32_t sector=first;sector<first+count;++sector) {
        if (expired(&deadline)) { status=NX_ERR_TIMEOUT; break; }
        nx_flash_block_t block;
        if (!nx_stm32f407_flash_block(&geometry,current_offset,&block)) {
            status=NX_ERR_INVALID_STATE; break;
        }
        FLASH_EraseInitTypeDef erase={.TypeErase=FLASH_TYPEERASE_SECTORS,
            .Sector=sector,.NbSectors=1,.VoltageRange=FLASH_VOLTAGE_RANGE_3};
        uint32_t failed=UINT32_MAX;
        HAL_StatusTypeDef hardware=HAL_FLASHEx_Erase(&erase,&failed);
        settle(); flush_cache();
        if (hardware!=HAL_OK) { status=hardware==HAL_TIMEOUT ? NX_ERR_TIMEOUT : NX_ERR_IO; break; }
        for (uint32_t i=0;i<block.size;i+=4U)
            if (*(volatile const uint32_t*)(geometry.base_address+block.offset+i)!=UINT32_MAX) { status=NX_ERR_IO; break; }
        if (status!=NX_OK) break;
        if (SystemCoreClock!=deadline.frequency) { status=NX_ERR_INVALID_STATE; break; }
        if (expired(&deadline)) { status=NX_ERR_TIMEOUT; break; }
        current_offset+=block.size;
    }
    give(); return status;
}
static nx_status_t sync_data(nx_flash_operations_t* self,uint32_t timeout) {
    (void)timeout;
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,false);
    if (status==NX_OK) { flush_cache(); give(); }
    return status;
}
static nx_status_t lock_device(nx_internal_flash_t* self) {
    if (self!=&device.api) return NX_ERR_INVALID_PARAM;
    nx_status_t status=take(true);
    if (status!=NX_OK) return status;
    if (HAL_FLASH_Lock()!=HAL_OK) status=NX_ERR_IO;
    else device.unlocked=false;
    give(); return status;
}
static nx_status_t unlock_device(nx_internal_flash_t* self) {
    if (self!=&device.api) return NX_ERR_INVALID_PARAM;
    nx_status_t status=take(true);
    if (status!=NX_OK) return status;
    nx_flash_geometry_t geometry;
    if (!physical_geometry(&geometry)) status=NX_ERR_INVALID_STATE;
    else if (HAL_FLASH_Unlock()!=HAL_OK) status=NX_ERR_IO;
    else device.unlocked=true;
    give(); return status;
}
static nx_status_t initialize(nx_lifecycle_t* self) {
    if (self!=&device.lifecycle) return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (device.initialized) return NX_ERR_ALREADY_INIT;
    nx_flash_geometry_t geometry;
    if (!physical_geometry(&geometry)) return NX_ERR_INVALID_STATE;
    if (HAL_FLASH_Lock()!=HAL_OK) return NX_ERR_IO;
    device.unlocked=false; device.initialized=true;
    return NX_OK;
}
static nx_status_t deinitialize(nx_lifecycle_t* self) {
    if (self!=&device.lifecycle) return NX_ERR_INVALID_PARAM;
    nx_status_t status=lock_device(&device.api);
    if (status!=NX_OK) return status;
    device.initialized=false;
    return NX_OK;
}
static nx_status_t unsupported_lifecycle(nx_lifecycle_t* self) {
    return self==&device.lifecycle ? NX_ERR_NOT_SUPPORTED : NX_ERR_INVALID_PARAM;
}
static nx_device_state_t state_query(nx_lifecycle_t* self) {
    return self!=&device.lifecycle ? NX_DEV_STATE_ERROR : device.initialized ? NX_DEV_STATE_RUNNING : NX_DEV_STATE_UNINITIALIZED;
}
static nx_lifecycle_t* lifecycle(nx_internal_flash_t* self) { return self==&device.api ? &device.lifecycle : NULL; }
static nx_flash_operations_t* operations(nx_internal_flash_t* self) { return self==&device.api ? &device.operations : NULL; }
/* Legacy absolute-address bridge does not round erase or silently unlock. */
static nx_status_t legacy_read(nx_internal_flash_t* self,uint32_t address,uint8_t* data,size_t n) {
    return self!=&device.api || address<0x08000000U ? NX_ERR_INVALID_PARAM : read_data(&device.operations,address-0x08000000U,data,n);
}
static nx_status_t legacy_write(nx_internal_flash_t* self,uint32_t address,const uint8_t* data,size_t n) {
    return self!=&device.api || address<0x08000000U ? NX_ERR_INVALID_PARAM : program_data(&device.operations,address-0x08000000U,data,n,UINT32_MAX);
}
static nx_status_t legacy_erase(nx_internal_flash_t* self,uint32_t address,size_t n) {
    return self!=&device.api || address<0x08000000U ? NX_ERR_INVALID_PARAM : erase_data(&device.operations,address-0x08000000U,n,UINT32_MAX);
}
static size_t page_size(nx_internal_flash_t* self) { (void)self; return 0; /* Non-uniform geometry: use typed get_block. */ }
static size_t write_size(nx_internal_flash_t* self) { return self==&device.api ? 4 : 0; }
static nx_status_t construct(const nx_device_t* descriptor,void** api) {
    if (!api) return NX_ERR_NULL_PTR;
    *api=NULL;
    if (!descriptor || descriptor->state!=&device.core) return NX_ERR_INVALID_PARAM;
    device.operations=(nx_flash_operations_t){.get_geometry=geometry_query,.get_block=block_query,
        .read=read_data,.program=program_data,.erase=erase_data,.sync=sync_data};
    device.lifecycle=(nx_lifecycle_t){.init=initialize,.deinit=deinitialize,
        .suspend=unsupported_lifecycle,.resume=unsupported_lifecycle,.get_state=state_query};
    NX_INIT_INTERNAL_FLASH(&device.api,legacy_read,legacy_write,legacy_erase,page_size,write_size,
                            lock_device,unlock_device,lifecycle);
    device.api.get_operations=operations;
    *api=&device.api; return NX_OK;
}
NX_DEVICE_REGISTER_TYPED(STM32_INTERNAL_FLASH,0,"FLASH0",NULL,&device.core,
    NX_DEVICE_CLASS_FLASH,NX_DEVICE_CAP_FLASH_GEOMETRY|NX_DEVICE_CAP_FLASH_PROGRAM|NX_DEVICE_CAP_FLASH_ERASE,construct,NULL);

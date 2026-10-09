/** Full GD32F470ZG physical Flash provider; no storage/product partition. */
#include "hal/provider/nx_device_provider.h"
#include "flash.h"
#include "flash_geometry.h"
#include "hal/base/nx_device.h"
#include "arch/nx_arch.h"
#include "gd32f470_platform.h"
#include "gd32f4xx.h"
#include <string.h>

static struct {
    nx_device_config_state_t core;
    nx_internal_flash_t api;
    nx_flash_operations_t operations;
    nx_lifecycle_t lifecycle;
    bool initialized, unlocked, busy;
} device;
static bool physical_geometry(nx_flash_geometry_t* geometry) {
    uint32_t bytes=(*(volatile const uint32_t*)0x1FFF7A20U>>16)*1024U;
    return nx_gd32f470_flash_geometry(bytes,geometry);
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
static bool expired(uint32_t start,uint32_t timeout) {
    return timeout!=UINT32_MAX && (uint32_t)(nx_gd32f470_millis()-start)>=timeout;
}
static void settle(void) {
    /* Do not hand the next owner a controller that still owns a write pulse. */
    while (fmc_state_get()==FMC_BUSY) { }
    nx_arch_dsb(); nx_arch_isb();
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
    return status!=NX_OK ? status : nx_gd32f470_flash_block(&geometry,offset,out) ? NX_OK : NX_ERR_INVALID_PARAM;
}
static nx_status_t read_data(nx_flash_operations_t* self,uint32_t offset,uint8_t* data,size_t length) {
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,false);
    if (status!=NX_OK) return status;
    if (!nx_gd32f470_flash_range(&geometry,offset,length) || (!data && length)) status=NX_ERR_INVALID_PARAM;
    else if (length) memcpy(data,(const void*)(geometry.base_address+offset),length);
    give(); return status;
}
static nx_status_t program_data(nx_flash_operations_t* self,uint32_t offset,
                                const uint8_t* data,size_t length,uint32_t timeout) {
    if (timeout!=UINT32_MAX && timeout>=0x80000000U) return NX_ERR_INVALID_PARAM;
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,true);
    if (status!=NX_OK) return status;
    if (!nx_gd32f470_flash_range(&geometry,offset,length) || (!data && length) || offset%2U || length%2U) {
        give(); return NX_ERR_INVALID_PARAM;
    }
    uint32_t start=nx_gd32f470_millis();
    fmc_flag_clear(FMC_FLAG_END|FMC_FLAG_OPERR|FMC_FLAG_WPERR|
                   FMC_FLAG_PGMERR|FMC_FLAG_PGSERR|FMC_FLAG_RDDERR);
    for (size_t i=0;i<length;i+=2U) {
        if (expired(start,timeout)) { status=NX_ERR_TIMEOUT; break; }
        uint16_t halfword; memcpy(&halfword,data+i,sizeof(halfword));
        uint32_t address=(uint32_t)geometry.base_address+offset+(uint32_t)i;
        if ((*(volatile const uint16_t*)(uintptr_t)address&halfword)!=halfword) { status=NX_ERR_INVALID_STATE; break; }
        fmc_state_enum hardware=fmc_halfword_program(address,halfword);
        settle();
        if (hardware!=FMC_READY) { status=hardware==FMC_TOERR ? NX_ERR_TIMEOUT : NX_ERR_IO; break; }
        if (*(volatile const uint16_t*)(uintptr_t)address!=halfword) { status=NX_ERR_IO; break; }
        if (expired(start,timeout)) { status=NX_ERR_TIMEOUT; break; }
    }
    give(); return status;
}
static nx_status_t erase_data(nx_flash_operations_t* self,uint32_t offset,size_t length,uint32_t timeout) {
    if (timeout!=UINT32_MAX && timeout>=0x80000000U) return NX_ERR_INVALID_PARAM;
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,true);
    if (status!=NX_OK) return status;
    if (!nx_gd32f470_flash_range(&geometry,offset,length) || offset%4096U || length%4096U) {
        give(); return NX_ERR_INVALID_PARAM;
    }
    uint32_t start=nx_gd32f470_millis();
    fmc_flag_clear(FMC_FLAG_END|FMC_FLAG_OPERR|FMC_FLAG_WPERR|
                   FMC_FLAG_PGMERR|FMC_FLAG_PGSERR|FMC_FLAG_RDDERR);
    for (size_t i=0;i<length;i+=4096U) {
        if (expired(start,timeout)) { status=NX_ERR_TIMEOUT; break; }
        uint32_t address=(uint32_t)geometry.base_address+offset+(uint32_t)i;
        fmc_state_enum hardware=fmc_page_erase(address);
        settle();
        if (hardware!=FMC_READY) { status=hardware==FMC_TOERR ? NX_ERR_TIMEOUT : NX_ERR_IO; break; }
        for (uint32_t j=0;j<4096U;j+=2U)
            if (*(volatile const uint16_t*)(uintptr_t)(address+j)!=UINT16_MAX) { status=NX_ERR_IO; break; }
        if (status!=NX_OK) break;
        if (expired(start,timeout)) { status=NX_ERR_TIMEOUT; break; }
    }
    give(); return status;
}
static nx_status_t sync_data(nx_flash_operations_t* self,uint32_t timeout) {
    (void)timeout;
    nx_flash_geometry_t geometry;
    nx_status_t status=begin(self,&geometry,false);
    if (status==NX_OK) { settle(); give(); }
    return status;
}
static nx_status_t lock_device(nx_internal_flash_t* self) {
    if (self!=&device.api) return NX_ERR_INVALID_PARAM;
    nx_status_t status=take(true);
    if (status!=NX_OK) return status;
    fmc_lock();
    if (!(FMC_CTL&FMC_CTL_LK)) status=NX_ERR_IO;
    else device.unlocked=false;
    give(); return status;
}
static nx_status_t unlock_device(nx_internal_flash_t* self) {
    if (self!=&device.api) return NX_ERR_INVALID_PARAM;
    nx_status_t status=take(true);
    if (status!=NX_OK) return status;
    nx_flash_geometry_t geometry;
    if (!physical_geometry(&geometry)) status=NX_ERR_INVALID_STATE;
    else {
        fmc_unlock();
        if (FMC_CTL&FMC_CTL_LK) status=NX_ERR_IO;
        else device.unlocked=true;
    }
    give(); return status;
}
static nx_status_t initialize(nx_lifecycle_t* self) {
    if (self!=&device.lifecycle) return NX_ERR_INVALID_PARAM;
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (device.initialized) return NX_ERR_ALREADY_INIT;
    nx_flash_geometry_t geometry;
    if (!physical_geometry(&geometry)) return NX_ERR_INVALID_STATE;
    fmc_lock();
    if (!(FMC_CTL&FMC_CTL_LK)) return NX_ERR_IO;
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
static nx_status_t legacy_read(nx_internal_flash_t* self,uint32_t address,uint8_t* data,size_t n) {
    return self!=&device.api || address<0x08000000U ? NX_ERR_INVALID_PARAM : read_data(&device.operations,address-0x08000000U,data,n);
}
static nx_status_t legacy_write(nx_internal_flash_t* self,uint32_t address,const uint8_t* data,size_t n) {
    return self!=&device.api || address<0x08000000U ? NX_ERR_INVALID_PARAM : program_data(&device.operations,address-0x08000000U,data,n,UINT32_MAX);
}
static nx_status_t legacy_erase(nx_internal_flash_t* self,uint32_t address,size_t n) {
    return self!=&device.api || address<0x08000000U ? NX_ERR_INVALID_PARAM : erase_data(&device.operations,address-0x08000000U,n,UINT32_MAX);
}
static size_t page_size(nx_internal_flash_t* self) { return self==&device.api ? 4096 : 0; }
static size_t write_size(nx_internal_flash_t* self) { return self==&device.api ? 2 : 0; }
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
NX_DEVICE_REGISTER_TYPED(GD32_INTERNAL_FLASH,0,"FLASH0",NULL,&device.core,
    NX_DEVICE_CLASS_FLASH,NX_DEVICE_CAP_FLASH_GEOMETRY|NX_DEVICE_CAP_FLASH_PROGRAM|NX_DEVICE_CAP_FLASH_ERASE,construct,NULL);

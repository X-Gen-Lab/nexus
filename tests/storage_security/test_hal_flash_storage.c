/** Production StorageCore -> StorageHAL -> Flash facade; explicit fault model. */
#include "nexus/hal_flash_storage.h"
#include "hal/provider/nx_device_provider.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d failed: %s\n",__FILE__,__LINE__,#x); abort(); } } while (0)
#define BYTES 1024u
static struct {
    nx_internal_flash_t api;
    nx_flash_operations_t ops;
    nx_lifecycle_t life;
    nx_device_state_t state;
    uint8_t bytes[BYTES];
    bool unlocked, nonuniform, reenter;
    uint8_t erased_value;
    uint32_t now, operation_delay, query_delay;
    nx_status_t clock_failure;
    long cut;
    size_t mutations;
} flash;
static nx_device_config_state_t storage;
static nx_device_t descriptor;
static nx_device_ref_t controller;
static nx_device_flash_region_t region;
static nx_hal_flash_storage_t adapter;
static nx_hal_flash_storage_clock_t clock_port;
static nx_flash_port_t port;
static bool bound;
static unsigned cases;
static nx_status_t init(nx_lifecycle_t* self) { (void)self;flash.state=NX_DEV_STATE_RUNNING;return NX_OK; }
static nx_status_t deinit(nx_lifecycle_t* self) { (void)self;flash.state=NX_DEV_STATE_UNINITIALIZED;return NX_OK; }
static nx_device_state_t state(nx_lifecycle_t* self) { (void)self;return flash.state; }
static nx_lifecycle_t* life(nx_internal_flash_t* self) { (void)self;return &flash.life; }
static nx_flash_operations_t* ops(nx_internal_flash_t* self) { (void)self;return &flash.ops; }
static nx_status_t construct(const nx_device_t* dev,void** api) { (void)dev;*api=&flash.api;return NX_OK; }
static nx_status_t geometry(nx_flash_operations_t* self,nx_flash_geometry_t* g) {
    (void)self;*g=(nx_flash_geometry_t){0x08000000,BYTES,4,flash.nonuniform?4u:8u,
        NX_FLASH_ERASE_STALLS_EXEC|NX_FLASH_PROGRAM_STALLS_EXEC,flash.erased_value};return NX_OK;
}
static nx_status_t block(nx_flash_operations_t* self,uint32_t at,nx_flash_block_t* b) {
    (void)self;flash.now+=flash.query_delay;
    if(at>=BYTES)return NX_ERR_INVALID_PARAM;
    if(!flash.nonuniform)*b=(nx_flash_block_t){at/128*128,128,at/128};
    else if(at<128)*b=(nx_flash_block_t){0,128,0};
    else if(at<256)*b=(nx_flash_block_t){128,128,1};
    else if(at<512)*b=(nx_flash_block_t){256,256,2};
    else *b=(nx_flash_block_t){512,512,3};
    return NX_OK;
}
static nx_status_t read(nx_flash_operations_t* self,uint32_t at,uint8_t* out,size_t size) {
    (void)self;if(at>=BYTES||size>BYTES-at)return NX_ERR_INVALID_PARAM;
    memcpy(out,flash.bytes+at,size);return NX_OK;
}
static bool mutation(void) {
    if(flash.cut>=0 && flash.mutations >= (size_t)flash.cut)return false;
    ++flash.mutations;return true;
}
static nx_status_t program(nx_flash_operations_t* self,uint32_t at,const uint8_t* data,size_t size,uint32_t budget) {
    (void)self;if(!flash.unlocked)return NX_ERR_PERMISSION;
    if(flash.reenter) {
        flash.reenter=false;
        uint8_t byte=0;
        CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_ERR_BUSY);
        CHECK(nx_device_flash_region_close(region)==NX_ERR_BUSY);
        CHECK(port.read(port.ctx,0,&byte,1)==NX_STORAGE_IO);
        CHECK(nx_hal_flash_storage_last_status(&adapter)==NX_ERR_BUSY);
    }
    flash.now+=flash.operation_delay;
    for(size_t i=0;i<size;++i) {
        if(!mutation())return NX_ERR_IO;
        if((flash.bytes[at+i]&data[i])!=data[i])return NX_ERR_IO;
        flash.bytes[at+i]&=data[i];
    }
    return flash.operation_delay>=budget?NX_ERR_TIMEOUT:NX_OK;
}
static nx_status_t erase(nx_flash_operations_t* self,uint32_t at,size_t size,uint32_t budget) {
    (void)self;if(!flash.unlocked)return NX_ERR_PERMISSION;
    flash.now+=flash.operation_delay;
    for(size_t i=0;i<size;++i){if(!mutation())return NX_ERR_IO;flash.bytes[at+i]=0xff;}
    return flash.operation_delay>=budget?NX_ERR_TIMEOUT:NX_OK;
}
static nx_status_t sync(nx_flash_operations_t* self,uint32_t budget) {
    (void)self;flash.now+=flash.operation_delay;
    if(!mutation())return NX_ERR_IO;
    return flash.operation_delay>=budget?NX_ERR_TIMEOUT:NX_OK;
}
static nx_status_t lock(nx_internal_flash_t* self) { (void)self;flash.unlocked=false;return NX_OK; }
static nx_status_t unlock(nx_internal_flash_t* self) { (void)self;flash.unlocked=true;return NX_OK; }
static nx_status_t now(void* ctx,uint32_t* out) { (void)ctx;*out=flash.now;return flash.clock_failure; }
static void attach(uint32_t permissions) {
    CHECK(nx_device_open("MODEL_FLASH",NX_DEVICE_CLASS_FLASH,0xf15,&controller)==NX_OK);
    CHECK(nx_device_flash_region_open(controller,0,BYTES,permissions,&region)==NX_OK);
}
static void bind(void) {
    CHECK(nx_hal_flash_storage_bind(&adapter,region,&clock_port,1000,&port)==NX_OK);bound=true;
}
static void detach(void) {
    if(bound) { if(adapter.active)CHECK(nx_hal_flash_storage_end(&adapter)==NX_OK);
        CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_OK);bound=false; }
    if(region.slot)CHECK(nx_device_flash_region_close(region)==NX_OK);
    if(controller.descriptor)CHECK(nx_device_close(controller)==NX_OK);
    region=(nx_device_flash_region_t){0};controller=(nx_device_ref_t){0};
}
static void fresh(void) {
    detach();memset(flash.bytes,0xff,sizeof(flash.bytes));flash.nonuniform=false;
    flash.erased_value=0xff;flash.reenter=false;
    flash.now=0;flash.operation_delay=0;flash.query_delay=0;flash.clock_failure=NX_OK;
    flash.cut=-1;flash.mutations=0;
    attach(NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE);bind();
}
static void begin(void) { CHECK(nx_hal_flash_storage_begin(&adapter,1000)==NX_OK); }
static void end(void) { CHECK(nx_hal_flash_storage_end(&adapter)==NX_OK); }
static void open_store(nx_storage_t* store) {
    begin();CHECK(nx_storage_open(store,&port,0,BYTES/2)==NX_STORAGE_OK);end();
}
static nx_storage_status_t save(nx_storage_t* store,const char* value,size_t len) {
    begin();nx_storage_status_t s=nx_storage_save(store,value,len);end();return s;
}
static void expect_value(nx_storage_t* store,const char* value,size_t len) {
    uint8_t actual[32];size_t size=sizeof(actual);begin();
    CHECK(nx_storage_load(store,actual,&size)==NX_STORAGE_OK);end();
    CHECK(size==len && memcmp(actual,value,len)==0);
}
static void normal_and_empty(void) {
    fresh();nx_storage_t store;open_store(&store);
    CHECK(port.size==BYTES && port.erase_size==128 && port.program_size==4);
    CHECK(nx_device_flash_set_write_enabled(controller,true)==NX_OK);
    flash.reenter=true;
    CHECK(save(&store,"durable",8)==NX_STORAGE_OK);CHECK(!flash.reenter);expect_value(&store,"durable",8);
    CHECK(save(&store,"",0)==NX_STORAGE_OK);expect_value(&store,"",0);
    CHECK(nx_device_flash_set_write_enabled(controller,false)==NX_OK);
}
static void explicit_unlock(void) {
    fresh();nx_storage_t store;open_store(&store);
    CHECK(save(&store,"blocked",8)==NX_STORAGE_IO);
    CHECK(nx_hal_flash_storage_last_status(&adapter)==NX_ERR_PERMISSION);
    CHECK(!flash.unlocked && !store.opened && flash.mutations==0);
}
static void reject_nonuniform_and_permission(void) {
    fresh();CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_OK);bound=false;
    CHECK(nx_device_flash_region_close(region)==NX_OK);region=(nx_device_flash_region_t){0};
    flash.nonuniform=true;
    CHECK(nx_device_flash_region_open(controller,0,BYTES,NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE,&region)==NX_OK);
    CHECK(nx_hal_flash_storage_bind(&adapter,region,&clock_port,1000,&port)==NX_ERR_NOT_SUPPORTED);
    CHECK(!port.ctx);CHECK(nx_device_flash_region_close(region)==NX_OK);region=(nx_device_flash_region_t){0};
    flash.nonuniform=false;flash.erased_value=0;
    CHECK(nx_device_flash_region_open(controller,0,BYTES,NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE,&region)==NX_OK);
    CHECK(nx_hal_flash_storage_bind(&adapter,region,&clock_port,1000,&port)==NX_ERR_NOT_SUPPORTED);
    CHECK(nx_device_flash_region_close(region)==NX_OK);region=(nx_device_flash_region_t){0};
    flash.erased_value=0xff;CHECK(nx_device_flash_region_open(controller,0,BYTES,NX_FLASH_REGION_READ,&region)==NX_OK);
    CHECK(nx_hal_flash_storage_bind(&adapter,region,&clock_port,1000,&port)==NX_ERR_PERMISSION);
    CHECK(nx_device_flash_region_close(region)==NX_OK);region=(nx_device_flash_region_t){0};
}
static void loans_and_stale_ports(void) {
    fresh();nx_flash_port_t old=port;
    CHECK(nx_device_flash_region_close(region)==NX_ERR_BUSY);
    begin();CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_ERR_BUSY);end();
    CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_OK);bound=false;
    uint8_t byte=0;CHECK(old.read(old.ctx,0,&byte,1)==NX_STORAGE_INVALID);
    bind();CHECK(old.ctx!=port.ctx);begin();
    CHECK(old.program(old.ctx,0,&byte,1)==NX_STORAGE_INVALID);end();
    CHECK(flash.mutations==0);
}
static void full_operation_deadline(void) {
    fresh();nx_storage_t store;open_store(&store);
    CHECK(nx_device_flash_set_write_enabled(controller,true)==NX_OK);
    flash.now=UINT32_MAX-1;flash.operation_delay=2;
    CHECK(nx_hal_flash_storage_begin(&adapter,3)==NX_OK);
    CHECK(nx_storage_save(&store,"new",4)==NX_STORAGE_IO);
    CHECK(nx_hal_flash_storage_last_status(&adapter)==NX_ERR_TIMEOUT);
    CHECK(!store.opened);CHECK(nx_hal_flash_storage_end(&adapter)==NX_OK);
    CHECK(nx_hal_flash_storage_begin(&adapter,0)==NX_ERR_INVALID_PARAM);
}
static void bounds_and_clock_failure(void) {
    fresh();uint8_t bytes[4]={0};begin();
    CHECK(port.program(port.ctx,BYTES,bytes,sizeof(bytes))==NX_STORAGE_INVALID);
    CHECK(port.read(port.ctx,BYTES,NULL,0)==NX_STORAGE_OK);
    CHECK(port.read(port.ctx,BYTES+1,NULL,0)==NX_STORAGE_INVALID);end();
    CHECK(flash.mutations==0);flash.clock_failure=NX_ERR_HARDWARE;
    CHECK(nx_hal_flash_storage_begin(&adapter,100)==NX_ERR_HARDWARE);
    flash.clock_failure=NX_OK;
    CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_OK);bound=false;
    flash.query_delay=10;CHECK(nx_hal_flash_storage_bind(&adapter,region,&clock_port,1,&port)==NX_ERR_TIMEOUT);
    flash.query_delay=0;CHECK(nx_device_flash_region_close(region)==NX_OK);region=(nx_device_flash_region_t){0};
}
static void stale_region(void) {
    fresh();CHECK(nx_hal_flash_storage_unbind(&adapter)==NX_OK);bound=false;
    nx_device_flash_region_t stale=region;CHECK(nx_device_flash_region_close(region)==NX_OK);
    region=(nx_device_flash_region_t){0};
    CHECK(nx_device_flash_region_open(controller,0,BYTES,NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE,&region)==NX_OK);
    CHECK(nx_hal_flash_storage_bind(&adapter,stale,&clock_port,1000,&port)==NX_ERR_INVALID_STATE);
    bind();
}
static void power_loss_boundaries(void) {
    fresh();nx_storage_t store;open_store(&store);
    CHECK(nx_device_flash_set_write_enabled(controller,true)==NX_OK);
    CHECK(save(&store,"original",9)==NX_STORAGE_OK);
    uint8_t original[BYTES];memcpy(original,flash.bytes,BYTES);
    flash.mutations=0;CHECK(save(&store,"replacement",12)==NX_STORAGE_OK);
    size_t boundaries=flash.mutations;
    CHECK(boundaries>0);
    for(size_t cut=0;cut<=boundaries;++cut) {
        detach();memcpy(flash.bytes,original,BYTES);flash.now=0;flash.cut=-1;flash.mutations=0;
        attach(NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE);bind();open_store(&store);
        CHECK(nx_device_flash_set_write_enabled(controller,true)==NX_OK);
        flash.cut=(long)cut;nx_storage_status_t changed=save(&store,"replacement",12);
        CHECK(changed==NX_STORAGE_OK || changed==NX_STORAGE_IO);
        if(changed==NX_STORAGE_IO)CHECK(!store.opened);
        detach();flash.cut=-1;
        attach(NX_FLASH_REGION_READ|NX_FLASH_REGION_PROGRAM|NX_FLASH_REGION_ERASE);bind();
        open_store(&store);uint8_t value[32];size_t size=sizeof(value);begin();
        CHECK(nx_storage_load(&store,value,&size)==NX_STORAGE_OK);end();
        CHECK((size==9 && memcmp(value,"original",9)==0) || (size==12 && memcmp(value,"replacement",12)==0));
    }
    printf("power-loss software boundaries: %zu\n",boundaries+1);
}
static void run(const char* name,void(*test)(void)) { test();++cases;printf("passed: %s\n",name); }
int main(void) {
    CHECK(nx_device_registry_reset()==NX_OK);flash.state=NX_DEV_STATE_UNINITIALIZED;
    flash.life=(nx_lifecycle_t){0};flash.life.init=init;flash.life.deinit=deinit;flash.life.get_state=state;
    flash.api=(nx_internal_flash_t){0};flash.api.get_operations=ops;flash.api.get_lifecycle=life;
    flash.api.lock=lock;flash.api.unlock=unlock;flash.ops=(nx_flash_operations_t){geometry,block,read,program,erase,sync};
    descriptor=(nx_device_t){0};descriptor.name="MODEL_FLASH";descriptor.state=&storage;
    descriptor.device_class=NX_DEVICE_CLASS_FLASH;descriptor.construct=construct;
    CHECK(nx_device_register(&descriptor)==NX_OK);clock_port=(nx_hal_flash_storage_clock_t){now,NULL};
    run("save/load and empty payload",normal_and_empty);run("explicit caller unlock",explicit_unlock);
    run("nonuniform/permission rejection",reject_nonuniform_and_permission);
    run("borrow lifecycle and old port identity",loans_and_stale_ports);
    run("one total deadline and clock wrap",full_operation_deadline);
    run("bounds, clock failure and validation deadline",bounds_and_clock_failure);
    run("stale region",stale_region);run("production storage power-loss recovery",power_loss_boundaries);
    detach();CHECK(nx_device_registry_reset()==NX_OK);printf("software scenarios passed: %u\n",cases);return 0;
}

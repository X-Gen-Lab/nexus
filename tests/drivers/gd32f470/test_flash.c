#define _GNU_SOURCE
#include "flash.h"
#include "identity.h"
#include "gd32f4xx.h"
#include "arch/nx_arch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
__asm__(".global __nexus_storage_start\n.set __nexus_storage_start,0x080FC000\n"
        ".global __nexus_storage_end\n.set __nexus_storage_end,0x08100000\n");
static bool locked=true,fail_program,fail_erase,skip_erase;
static uint32_t programs,erases,ipsr;
static uint32_t pages[4];
bool nx_arch_in_isr(void){return ipsr!=0;}
void nx_arch_dsb(void){}
void fmc_unlock(void){locked=false;}
void fmc_lock(void){locked=true;}
void fmc_flag_clear(uint32_t x){assert(x);}
fmc_state_enum fmc_state_get(void){return FMC_READY;}
fmc_state_enum fmc_halfword_program(uint32_t address,uint16_t value){
    assert(!locked&&address>=0x080FC000&&address+2u<=0x08100000);
    programs++;if(fail_program)return FMC_OPERR;
    *(uint16_t*)(uintptr_t)address&=value;return FMC_READY;
}
fmc_state_enum fmc_page_erase(uint32_t address){
    /* Independent page model: a 128KiB sector erase would touch the canary. */
    assert(!locked&&address>=0x080FC000&&address+4096u<=0x08100000&&address%4096u==0);
    if(erases<4) { pages[erases]=address; }
    erases++;
    if(fail_erase)return FMC_OPERR;
    if(!skip_erase)memset((void*)(uintptr_t)address,0xFF,4096);
    return FMC_READY;
}
static void map(uintptr_t address,size_t size){
    assert(mmap((void*)address,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)address);
}
int main(void){
    map(0x08000000,1048576);map(0x1FFF7000,4096);
    memset((void*)0x08000000,0xA5,1048576);
    *(uint32_t*)0x1FFF7A20=(1024u<<16)|512u;
    uint32_t* uid=(uint32_t*)0x1FFF7A10;
    uid[0]=0x11111111;uid[1]=0x22222222;uid[2]=0x33333333;
    nx_gd32f470_identity_t identity;
    nx_gd32f470_identity(&identity);
    assert(identity.uid[0]==uid[0]&&identity.uid[1]==uid[1]&&identity.uid[2]==uid[2]);
    assert(identity.flash_kib==1024&&identity.sram_kib==512&&identity.silicon_id==DBG_ID);
    const nx_flash_port_t* port=nx_gd32f470_flash_port();
    assert(port&&port->size==16384&&port->erase_size==4096&&port->program_size==2);
    assert(port->erase(port->ctx,0,16384)==NX_STORAGE_OK&&locked&&erases==4);
    for(unsigned i=0;i<4;i++)assert(pages[i]==0x080FC000+i*4096);
    for(uintptr_t p=0x08000000;p<0x080FC000;p++)assert(*(uint8_t*)p==0xA5); /* firmware canary */
    uint16_t value=0x1234,actual=0;
    assert(port->program(port->ctx,16382,&value,2)==NX_STORAGE_OK&&locked);
    assert(port->read(port->ctx,16382,&actual,2)==NX_STORAGE_OK&&actual==value);
    uint32_t count=programs;
    value=0xFFFF;assert(port->program(port->ctx,16382,&value,2)==NX_STORAGE_IO&&programs==count&&locked);
    assert(port->erase(port->ctx,1,4096)==NX_STORAGE_INVALID);
    assert(port->erase(port->ctx,0,2048)==NX_STORAGE_INVALID);
    assert(port->erase(port->ctx,16384,4096)==NX_STORAGE_INVALID);
    assert(port->read(port->ctx,SIZE_MAX,&actual,2)==NX_STORAGE_INVALID);
    assert(port->program(port->ctx,16383,&actual,2)==NX_STORAGE_INVALID);
    fail_erase=true;assert(port->erase(port->ctx,0,4096)==NX_STORAGE_IO&&locked);fail_erase=false;
    *(uint8_t*)0x080FC000=0;skip_erase=true;assert(port->erase(port->ctx,0,4096)==NX_STORAGE_IO&&locked);skip_erase=false;
    fail_program=true;assert(port->program(port->ctx,2,&actual,2)==NX_STORAGE_IO&&locked);fail_program=false;
    ipsr=1;assert(port->erase(port->ctx,0,4096)==NX_STORAGE_INVALID&&port->sync(port->ctx)==NX_STORAGE_INVALID);ipsr=0;
    *(uint32_t*)0x1FFF7A20=(512u<<16)|256u;assert(nx_gd32f470_flash_port()==NULL);
    puts("GD32 4KiB independent page erase, firmware canary, physical density, bounds and I/O faults passed");
    return 0;
}

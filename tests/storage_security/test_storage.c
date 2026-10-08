#define _POSIX_C_SOURCE 200809L
#include "nexus/file_flash.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed %s:%d: %s\n", __FILE__,__LINE__,#x); abort(); } } while (0)
static char path[128];
static nx_file_flash_t flash;
static nx_storage_t store;
static void open_store(void) {
    CHECK(nx_file_flash_open(&flash,path,512,128,8) == NX_STORAGE_OK);
    CHECK(nx_storage_open(&store,&flash.port,0,256) == NX_STORAGE_OK);
}
static void fresh(void) { unlink(path); open_store(); }
static void expect_value(const char* a, const char* b) {
    char value[100] = {0}; size_t size = sizeof(value);
    CHECK(nx_storage_load(&store,value,&size) == NX_STORAGE_OK);
    CHECK((size == strlen(a)+1 && !memcmp(value,a,size)) ||
          (b && size == strlen(b)+1 && !memcmp(value,b,size)));
}
static void power_loss_matrix(void) {
    fresh(); CHECK(nx_storage_save(&store,"original",9) == NX_STORAGE_OK);
    nx_file_flash_fail_after(&flash,-1);
    CHECK(nx_storage_save(&store,"replacement",12) == NX_STORAGE_OK);
    uint64_t events = flash.mutation_events;
    nx_file_flash_close(&flash);
    for (uint64_t point=0; point <= events; ++point) {
        fresh(); CHECK(nx_storage_save(&store,"original",9) == NX_STORAGE_OK);
        nx_file_flash_fail_after(&flash,(int64_t)point);
        nx_storage_status_t status = nx_storage_save(&store,"replacement",12);
        CHECK(status == NX_STORAGE_IO || status == NX_STORAGE_OK);
        if (status == NX_STORAGE_IO) {
            char stale[32]; size_t stale_size = sizeof(stale);
            CHECK(!store.opened);
            CHECK(nx_storage_load(&store,stale,&stale_size)==NX_STORAGE_INVALID);
        }
        nx_file_flash_close(&flash); open_store(); expect_value("original","replacement");
        nx_file_flash_close(&flash);
    }
    printf("storage overwrite: %llu interruption boundaries passed\n",
           (unsigned long long)(events+1));
    for (uint64_t point=0; point <= events; ++point) {
        fresh(); nx_file_flash_fail_after(&flash,(int64_t)point);
        nx_storage_status_t status = nx_storage_save(&store,"replacement",12);
        CHECK(status == NX_STORAGE_IO || status == NX_STORAGE_OK);
        nx_file_flash_close(&flash); open_store();
        char value[100]; size_t size=sizeof(value);
        status = nx_storage_load(&store,value,&size);
        CHECK(status == NX_STORAGE_NOT_FOUND ||
              (status == NX_STORAGE_OK && size==12 && !memcmp(value,"replacement",12)));
        nx_file_flash_close(&flash);
    }
    printf("storage first commit: %llu interruption boundaries passed\n",
           (unsigned long long)(events+1));
}
static void geometry_errors(void) {
    fresh(); CHECK(nx_storage_capacity(&store)==216);
    uint8_t ones[8]; memset(ones,0xff,sizeof(ones));
    uint8_t zeros[8]={0};
    CHECK(flash.port.program(&flash,0,zeros,8)==NX_STORAGE_OK);
    CHECK(flash.port.program(&flash,0,ones,8)==NX_STORAGE_INVALID);
    CHECK(flash.port.program(&flash,1,zeros,8)==NX_STORAGE_INVALID);
    CHECK(flash.port.erase(&flash,0,3)==NX_STORAGE_INVALID);
    CHECK(nx_storage_save(&store,ones,SIZE_MAX)==NX_STORAGE_NO_SPACE);
    CHECK(nx_storage_save(&store,NULL,1)==NX_STORAGE_INVALID);
    nx_file_flash_close(&flash);
    CHECK(nx_file_flash_open(&flash,path,1024,128,8)==NX_STORAGE_INVALID);
}
static void aliased_port_reopen(void) {
    fresh(); CHECK(nx_storage_save(&store,"durable",8)==NX_STORAGE_OK);
    CHECK(nx_storage_open(&store,&store.flash,store.offset,store.bank_size)==NX_STORAGE_OK);
    expect_value("durable",NULL);
    nx_file_flash_fail_after(&flash,0);
    CHECK(nx_storage_save(&store,"replacement",12)==NX_STORAGE_IO);
    CHECK(!store.opened);
    nx_file_flash_fail_after(&flash,-1);
    CHECK(nx_storage_open(&store,&store.flash,store.offset,store.bank_size)==NX_STORAGE_OK);
    expect_value("durable",NULL);
    nx_file_flash_close(&flash);
}
static void corruption_recovery(void) {
    fresh(); CHECK(nx_storage_save(&store,"original",9)==NX_STORAGE_OK);
    CHECK(nx_storage_save(&store,"replacement",12)==NX_STORAGE_OK);
    uint8_t damage=0;
    CHECK(pwrite(flash.fd,&damage,1,256+store.payload_offset)==1);
    nx_file_flash_close(&flash); open_store(); expect_value("original",NULL);
    CHECK(pwrite(flash.fd,&damage,1,store.payload_offset)==1);
    nx_file_flash_close(&flash);
    CHECK(nx_file_flash_open(&flash,path,512,128,8)==NX_STORAGE_OK);
    CHECK(nx_storage_open(&store,&flash.port,0,256)==NX_STORAGE_CORRUPT);
    nx_file_flash_close(&flash);
}
static void process_persistence(const char* self) {
    fresh(); CHECK(nx_storage_save(&store,"process durable",16)==NX_STORAGE_OK);
    nx_file_flash_close(&flash);
    pid_t pid=fork(); CHECK(pid>=0);
    if (!pid) { execl(self,self,"--read",path,(char*)NULL); _exit(127); }
    int status; CHECK(waitpid(pid,&status,0)==pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status)==0);
}
int main(int argc,char** argv) {
    if (argc==3 && !strcmp(argv[1],"--read")) {
        snprintf(path,sizeof(path),"%s",argv[2]); open_store();
        expect_value("process durable",NULL); nx_file_flash_close(&flash); return 0;
    }
    snprintf(path,sizeof(path),"/tmp/nexus-storage-%ld.flash",(long)getpid());
    power_loss_matrix(); geometry_errors(); aliased_port_reopen(); corruption_recovery();
    process_persistence(argv[0]); unlink(path);
    puts("storage geometry, corruption, exhausted capacity, erase/program failures and cross-process persistence passed");
    return 0;
}

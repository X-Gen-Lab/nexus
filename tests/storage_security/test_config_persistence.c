#define _POSIX_C_SOURCE 200809L
#include "config/config.h"
#include "config/config_backend.h"
#include "config_store.h"
#include "nexus/file_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed %s:%d: %s\n", __FILE__,__LINE__,#x); abort(); } } while (0)
static char path[128];
static nx_file_flash_t flash;
static nx_storage_t store;
static const uint8_t old_key[32]={1,4,7,9,2,8,3,6,5};
static const uint8_t new_key[32]={7,9,1,5,4,3,2,6,8};
static bool enable_auto;
static void start(bool fresh) {
    if (fresh) unlink(path);
    CHECK(nx_file_flash_open(&flash,path,1024,128,8)==NX_STORAGE_OK);
    CHECK(nx_storage_open(&store,&flash.port,0,512)==NX_STORAGE_OK);
    CHECK(config_backend_flash_bind(&store)==CONFIG_OK);
    config_manager_config_t cfg = CONFIG_MANAGER_CONFIG_DEFAULT;
    cfg.auto_commit = enable_auto;
    CHECK(config_init(&cfg)==CONFIG_OK);
    CHECK(config_register_encryption_key(old_key,32,CONFIG_CRYPTO_AES256_GCM,true)==CONFIG_OK);
    CHECK(config_register_encryption_key(new_key,32,CONFIG_CRYPTO_AES256_GCM,false)==CONFIG_OK);
    CHECK(config_set_backend(config_backend_flash_get())==CONFIG_OK);
}
static void stop(void) {
    CHECK(config_deinit()==CONFIG_OK);
    CHECK(config_backend_flash_bind(NULL)==CONFIG_OK);
    nx_file_flash_close(&flash);
}
static void populate(void) {
    CHECK(config_set_str_encrypted("device.secret","industrial-device")==CONFIG_OK);
    CHECK(config_set_str_encrypted("network.token","network-credential")==CONFIG_OK);
    CHECK(config_commit()==CONFIG_OK);
}
static void verify(void) {
    char out[100];
    CHECK(config_get_str("device.secret",out,sizeof(out))==CONFIG_OK);
    CHECK(!strcmp(out,"industrial-device"));
    CHECK(config_get_str("network.token",out,sizeof(out))==CONFIG_OK);
    CHECK(!strcmp(out,"network-credential"));
    uint8_t id[16], value[256]; size_t n=sizeof(value);
    CHECK(config_get_encryption_key_id(store.generation==1?old_key:new_key,32,
          CONFIG_CRYPTO_AES256_GCM,id)==CONFIG_OK);
    CHECK(config_store_get("device.secret",NULL,value,&n,NULL,0)==CONFIG_OK);
    CHECK(!memcmp(value+8,id,16));
    n=sizeof(value);
    CHECK(config_store_get("network.token",NULL,value,&n,NULL,0)==CONFIG_OK);
    CHECK(!memcmp(value+8,id,16));
}
static void rotation_power_loss(void) {
    start(true); populate(); nx_file_flash_fail_after(&flash,-1);
    CHECK(config_rotate_encryption_key(new_key,32,CONFIG_CRYPTO_AES256_GCM)==CONFIG_OK);
    uint64_t events=flash.mutation_events; stop();
    for(uint64_t point=0; point<=events; ++point) {
        start(true); populate();
        nx_file_flash_fail_after(&flash,(int64_t)point);
        config_status_t status=config_rotate_encryption_key(new_key,32,CONFIG_CRYPTO_AES256_GCM);
        CHECK(status==CONFIG_OK || status==CONFIG_ERROR_NVS_WRITE);
        char out[100];
        CHECK(config_get_str("device.secret",out,sizeof(out))==CONFIG_OK);
        CHECK(!strcmp(out,"industrial-device"));
        stop(); start(false); CHECK(config_load()==CONFIG_OK); verify(); stop();
    }
    printf("config atomic key rotation: %llu interruption boundaries passed\n",
           (unsigned long long)(events+1));
}
static void deletion_namespace_roundtrip(void) {
    start(true);
    CHECK(config_set_i32("same.key",11)==CONFIG_OK);
    config_ns_handle_t ns;
    CHECK(config_open_namespace("motor",&ns)==CONFIG_OK);
    CHECK(config_ns_set_i32(ns,"same.key",22)==CONFIG_OK);
    CHECK(config_close_namespace(ns)==CONFIG_OK);
    CHECK(config_set_str("obsolete","remove-me")==CONFIG_OK);
    CHECK(config_commit()==CONFIG_OK);
    CHECK(config_delete("obsolete")==CONFIG_OK);
    CHECK(config_commit()==CONFIG_OK); stop();
    start(false); CHECK(config_load()==CONFIG_OK);
    bool exists=true; CHECK(config_exists("obsolete",&exists)==CONFIG_OK && !exists);
    int32_t n=0; CHECK(config_get_i32("same.key",&n,0)==CONFIG_OK && n==11);
    CHECK(config_open_namespace("motor",&ns)==CONFIG_OK);
    CHECK(config_ns_get_i32(ns,"same.key",&n,0)==CONFIG_OK && n==22);
    CHECK(config_close_namespace(ns)==CONFIG_OK); stop();
}
static void process_roundtrip(const char* self) {
    start(true); populate(); CHECK(config_rotate_encryption_key(new_key,32,
         CONFIG_CRYPTO_AES256_GCM)==CONFIG_OK); stop();
    pid_t pid=fork(); CHECK(pid>=0);
    if(!pid) { execl(self,self,"--read",path,(char*)NULL); _exit(127); }
    int status; CHECK(waitpid(pid,&status,0)==pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status)==0);
}
static void malformed_preserves_live_data(void) {
    start(true); CHECK(config_set_i32("live",91)==CONFIG_OK);
    const uint8_t bad[]={ 'N','X','C','S',1,1,1,0,0,7,'d','e','f','a','u','l','t',
                         0,0,0,4,4,0,'e','v','i','l',1,2,3 }; /* truncated */
    CHECK(nx_storage_save(&store,bad,sizeof(bad))==NX_STORAGE_OK);
    CHECK(config_load()==CONFIG_ERROR_INVALID_FORMAT);
    int32_t value; CHECK(config_get_i32("live",&value,0)==CONFIG_OK && value==91);
    stop();
    CHECK(config_init(NULL)==CONFIG_OK);
    CHECK(config_set_backend(config_backend_flash_get())==CONFIG_ERROR_UNSUPPORTED);
    CHECK(config_deinit()==CONFIG_OK);
}
static void auto_commit_roundtrip(void) {
    enable_auto=true; start(true);
    CHECK(config_set_i32("auto",10)==CONFIG_OK);
    CHECK(store.generation==1);
    config_ns_handle_t ns;
    CHECK(config_open_namespace("motor",&ns)==CONFIG_OK);
    CHECK(config_ns_set_i32(ns,"auto",20)==CONFIG_OK);
    CHECK(store.generation==2);
    CHECK(config_close_namespace(ns)==CONFIG_OK);
    CHECK(config_set_str_encrypted("secret","auto-commit")==CONFIG_OK);
    CHECK(store.generation==3); /* Exactly one snapshot per public setter. */
    CHECK(config_set_str("secret","plaintext-downgrade")==CONFIG_ERROR_INVALID_PARAM);
    CHECK(store.generation==3);
    stop(); start(false); CHECK(config_load()==CONFIG_OK);
    int32_t n; CHECK(config_get_i32("auto",&n,0)==CONFIG_OK && n==10);
    CHECK(config_open_namespace("motor",&ns)==CONFIG_OK);
    CHECK(config_ns_get_i32(ns,"auto",&n,0)==CONFIG_OK && n==20);
    CHECK(config_close_namespace(ns)==CONFIG_OK);
    nx_file_flash_fail_after(&flash,0);
    CHECK(config_set_i32("auto",30)==CONFIG_ERROR_NVS_WRITE);
    stop(); start(false); CHECK(config_load()==CONFIG_OK);
    CHECK(config_get_i32("auto",&n,0)==CONFIG_OK && n==10);
    CHECK(config_delete("auto")==CONFIG_OK);
    stop(); start(false); CHECK(config_load()==CONFIG_OK);
    bool exists=true; CHECK(config_exists("auto",&exists)==CONFIG_OK && !exists);
    stop(); enable_auto=false;
}
static config_backend_snapshot_save_fn delegated_save;
static config_status_t reentrant_save(void* ctx,const void* data,size_t size) {
    CHECK(config_commit()==CONFIG_ERROR_BUSY);
    CHECK(config_load()==CONFIG_ERROR_BUSY);
    CHECK(config_set_backend(config_backend_flash_get())==CONFIG_ERROR_BUSY);
    CHECK(config_deinit()==CONFIG_ERROR_BUSY && config_is_initialized());
    return delegated_save(ctx,data,size);
}
static void readonly_and_reentrancy(void) {
    start(true);
    int32_t v=7;
    CHECK(config_store_set("immutable",CONFIG_TYPE_I32,&v,sizeof(v),
          CONFIG_FLAG_READONLY,0)==CONFIG_OK);
    CHECK(config_set_i32("immutable",8)==CONFIG_ERROR_READ_ONLY);
    CHECK(config_delete("immutable")==CONFIG_ERROR_READ_ONLY);
    CHECK(config_store_clear_all()==CONFIG_ERROR_READ_ONLY);
    config_backend_t reentrant=*config_backend_flash_get();
    delegated_save=reentrant.save_snapshot; reentrant.save_snapshot=reentrant_save;
    CHECK(config_set_backend(&reentrant)==CONFIG_OK);
    CHECK(config_commit()==CONFIG_OK);
    CHECK(config_load()==CONFIG_OK);
    CHECK(config_get_i32("immutable",&v,0)==CONFIG_OK && v==7);
    stop();
}
static bool block_deinit;
static config_backend_deinit_fn delegated_deinit;
static config_status_t failing_deinit(void* ctx) {
    return block_deinit ? CONFIG_ERROR_NVS_WRITE : delegated_deinit(ctx);
}
static void deinit_failure_retains_owner(void) {
    start(true); populate();
    config_backend_t owned=*config_backend_flash_get();
    delegated_deinit=owned.deinit; owned.deinit=failing_deinit;
    CHECK(config_set_backend(&owned)==CONFIG_OK); block_deinit=true;
    CHECK(config_deinit()==CONFIG_ERROR_NVS_WRITE);
    CHECK(config_is_initialized());
    char secret[100];
    CHECK(config_get_str("device.secret",secret,sizeof(secret))==CONFIG_OK);
    CHECK(!strcmp(secret,"industrial-device"));
    CHECK(config_init(NULL)==CONFIG_ERROR_ALREADY_INIT);
    block_deinit=false; stop();
}
int main(int argc,char** argv) {
    if(argc==3 && !strcmp(argv[1],"--read")) {
        snprintf(path,sizeof(path),"%s",argv[2]); start(false);
        CHECK(config_load()==CONFIG_OK); verify(); stop(); return 0;
    }
    snprintf(path,sizeof(path),"/tmp/nexus-config-persistence-%ld.flash",(long)getpid());
    rotation_power_loss(); deletion_namespace_roundtrip();
    process_roundtrip(argv[0]); malformed_preserves_live_data();
    auto_commit_roundtrip(); readonly_and_reentrancy();
    deinit_failure_retains_owner(); unlink(path);
    puts("config deletion, namespaces, cross-process keys, malformed snapshots and unavailable flash passed");
    return 0;
}

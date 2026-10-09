/**
 * \file            components_storage.c
 * \brief           Actual persistent two-bank corruption and interruption tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define _POSIX_C_SOURCE 200809L
#include "nexus/components/file_flash.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x)                                                               \
    do {                                                                       \
        if (!(x)) {                                                            \
            fprintf(stderr, "check failed %s:%d: %s\n", __FILE__, __LINE__,    \
                    #x);                                                       \
            abort();                                                           \
        }                                                                      \
    } while (0)
static char s_path[128];
static nx_file_flash_t s_flash;
static nx_storage_t s_store;
static uint8_t s_workspace[32];
/** \brief Open store. */
static void open_store(void) {
    CHECK(nx_file_flash_open(&s_flash, s_path, 512, 128, 8) == NX_STORAGE_OK);
    CHECK(nx_storage_open(&s_store, &s_flash.port, 0, 256, s_workspace,
                          sizeof(s_workspace)) == NX_STORAGE_OK);
}
/** \brief Fresh. */
static void fresh(void) {
    unlink(s_path);
    open_store();
}
/** \brief Expect value. */
static void expect_value(const char* a, const char* b) {
    char value[100] = {0};
    size_t size = sizeof(value);
    CHECK(nx_storage_load(&s_store, value, &size) == NX_STORAGE_OK);
    CHECK((size == strlen(a) + 1 && !memcmp(value, a, size)) ||
          (b && size == strlen(b) + 1 && !memcmp(value, b, size)));
}
/** \brief Power loss matrix. */
static void power_loss_matrix(void) {
    fresh();
    CHECK(nx_storage_save(&s_store, "original", 9) == NX_STORAGE_OK);
    nx_file_flash_fail_after(&s_flash, -1);
    CHECK(nx_storage_save(&s_store, "replacement", 12) == NX_STORAGE_OK);
    uint64_t events = s_flash.mutation_events;
    nx_file_flash_close(&s_flash);
    for (uint64_t point = 0; point <= events; ++point) {
        fresh();
        CHECK(nx_storage_save(&s_store, "original", 9) == NX_STORAGE_OK);
        nx_file_flash_fail_after(&s_flash, (int64_t)point);
        nx_storage_status_t status =
            nx_storage_save(&s_store, "replacement", 12);
        CHECK(status == NX_STORAGE_IO || status == NX_STORAGE_OK);
        if (status == NX_STORAGE_IO) {
            char stale[32];
            size_t stale_size = sizeof(stale);
            CHECK(!s_store.opened);
            CHECK(nx_storage_load(&s_store, stale, &stale_size) ==
                  NX_STORAGE_INVALID);
        }
        nx_file_flash_close(&s_flash);
        open_store();
        expect_value("original", "replacement");
        nx_file_flash_close(&s_flash);
    }
    printf("storage overwrite: %llu interruption boundaries passed\n",
           (unsigned long long)(events + 1));
    for (uint64_t point = 0; point <= events; ++point) {
        fresh();
        nx_file_flash_fail_after(&s_flash, (int64_t)point);
        nx_storage_status_t status =
            nx_storage_save(&s_store, "replacement", 12);
        CHECK(status == NX_STORAGE_IO || status == NX_STORAGE_OK);
        nx_file_flash_close(&s_flash);
        open_store();
        char value[100];
        size_t size = sizeof(value);
        status = nx_storage_load(&s_store, value, &size);
        CHECK(status == NX_STORAGE_NOT_FOUND ||
              (status == NX_STORAGE_OK && size == 12 &&
               !memcmp(value, "replacement", 12)));
        nx_file_flash_close(&s_flash);
    }
    printf("storage first commit: %llu interruption boundaries passed\n",
           (unsigned long long)(events + 1));
}
/** \brief Geometry errors. */
static void geometry_errors(void) {
    fresh();
    CHECK(nx_storage_capacity(&s_store) == 216);
    uint8_t ones[8];
    memset(ones, 0xff, sizeof(ones));
    uint8_t zeros[8] = {0};
    CHECK(s_flash.port.program(&s_flash, 0, zeros, 8) == NX_STORAGE_OK);
    CHECK(s_flash.port.program(&s_flash, 0, ones, 8) == NX_STORAGE_INVALID);
    CHECK(s_flash.port.program(&s_flash, 1, zeros, 8) == NX_STORAGE_INVALID);
    CHECK(s_flash.port.erase(&s_flash, 0, 3) == NX_STORAGE_INVALID);
    CHECK(nx_storage_save(&s_store, ones, SIZE_MAX) == NX_STORAGE_NO_SPACE);
    CHECK(nx_storage_save(&s_store, NULL, 1) == NX_STORAGE_INVALID);
    nx_file_flash_close(&s_flash);
    CHECK(nx_file_flash_open(&s_flash, s_path, 1024, 128, 8) ==
          NX_STORAGE_INVALID);
}
/** \brief Aliased port reopen. */
static void aliased_port_reopen(void) {
    fresh();
    CHECK(nx_storage_save(&s_store, "durable", 8) == NX_STORAGE_OK);
    CHECK(nx_storage_open(&s_store, &s_store.flash, s_store.offset,
                          s_store.bank_size, s_workspace,
                          sizeof(s_workspace)) == NX_STORAGE_OK);
    expect_value("durable", NULL);
    nx_file_flash_fail_after(&s_flash, 0);
    CHECK(nx_storage_save(&s_store, "replacement", 12) == NX_STORAGE_IO);
    CHECK(!s_store.opened);
    nx_file_flash_fail_after(&s_flash, -1);
    CHECK(nx_storage_open(&s_store, &s_store.flash, s_store.offset,
                          s_store.bank_size, s_workspace,
                          sizeof(s_workspace)) == NX_STORAGE_OK);
    expect_value("durable", NULL);
    nx_file_flash_close(&s_flash);
}
/** \brief Corruption recovery. */
static void corruption_recovery(void) {
    fresh();
    CHECK(nx_storage_save(&s_store, "original", 9) == NX_STORAGE_OK);
    CHECK(nx_storage_save(&s_store, "replacement", 12) == NX_STORAGE_OK);
    uint8_t damage = 0;
    CHECK(pwrite(s_flash.fd, &damage, 1,
                 (off_t)(256 + s_store.payload_offset)) == 1);
    nx_file_flash_close(&s_flash);
    open_store();
    expect_value("original", NULL);
    CHECK(pwrite(s_flash.fd, &damage, 1, (off_t)s_store.payload_offset) == 1);
    nx_file_flash_close(&s_flash);
    CHECK(nx_file_flash_open(&s_flash, s_path, 512, 128, 8) == NX_STORAGE_OK);
    CHECK(nx_storage_open(&s_store, &s_flash.port, 0, 256, s_workspace,
                          sizeof(s_workspace)) == NX_STORAGE_CORRUPT);
    nx_file_flash_close(&s_flash);
}
/** \brief Process persistence. */
static void process_persistence(const char* self) {
    fresh();
    CHECK(nx_storage_save(&s_store, "process durable", 16) == NX_STORAGE_OK);
    nx_file_flash_close(&s_flash);
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (!pid) {
        execl(self, self, "--read", s_path, (char*)NULL);
        _exit(127);
    }
    int status;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
int main(int argc, char** argv) {
    if (argc == 3 && !strcmp(argv[1], "--read")) {
        snprintf(s_path, sizeof(s_path), "%s", argv[2]);
        open_store();
        expect_value("process durable", NULL);
        nx_file_flash_close(&s_flash);
        return 0;
    }
    snprintf(s_path, sizeof(s_path), "/tmp/nexus-storage-%ld.s_flash",
             (long)getpid());
    power_loss_matrix();
    geometry_errors();
    aliased_port_reopen();
    corruption_recovery();
    process_persistence(argv[0]);
    unlink(s_path);
    puts("storage geometry, corruption, exhausted capacity, erase/program "
         "failures and cross-process persistence passed");
    return 0;
}

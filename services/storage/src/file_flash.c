#define _POSIX_C_SOURCE 200809L
#include "nexus/file_flash.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static bool range(nx_file_flash_t* f, size_t off, size_t n) {
    return f && f->fd >= 0 && off <= f->port.size && n <= f->port.size - off;
}
static bool event(nx_file_flash_t* f) {
    if (f->fail_after == 0) return false;
    if (f->fail_after > 0) --f->fail_after;
    ++f->mutation_events;
    return true;
}
static bool read_full(int fd, void* out, size_t len, size_t off) {
    size_t n = 0;
    while (n < len) {
        ssize_t got = pread(fd, (uint8_t*)out + n, len - n, (off_t)(off + n));
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return false;
        n += (size_t)got;
    }
    return true;
}
static bool write_full(int fd, const void* in, size_t len, size_t off) {
    size_t n = 0;
    while (n < len) {
        ssize_t got = pwrite(fd, (const uint8_t*)in + n, len - n,
                             (off_t)(off + n));
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) return false;
        n += (size_t)got;
    }
    return true;
}
static nx_storage_status_t mutate(nx_file_flash_t* f, size_t off,
                                  const void* data, size_t n) {
    size_t allowed = n;
    if (f->fail_after >= 0 && (uint64_t)f->fail_after < n)
        allowed = (size_t)f->fail_after;
    if (f->fail_after >= 0) f->fail_after -= (int64_t)allowed;
    f->mutation_events += allowed;
    if (allowed && !write_full(f->fd, data, allowed, off)) return NX_STORAGE_IO;
    return allowed == n ? NX_STORAGE_OK : NX_STORAGE_IO;
}
static nx_storage_status_t file_read(void* ctx, size_t off, void* out, size_t n) {
    nx_file_flash_t* f = ctx;
    if (!range(f, off, n) || (!out && n)) return NX_STORAGE_INVALID;
    return read_full(f->fd, out, n, off) ? NX_STORAGE_OK : NX_STORAGE_IO;
}
static nx_storage_status_t file_program(void* ctx, size_t off, const void* in,
                                        size_t n) {
    nx_file_flash_t* f = ctx;
    if (!range(f, off, n) || (!in && n) || off % f->port.program_size ||
        n % f->port.program_size) return NX_STORAGE_INVALID;
    uint8_t old[NX_STORAGE_MAX_PROGRAM_SIZE];
    for (size_t pos = 0; pos < n; pos += f->port.program_size) {
        if (!read_full(f->fd, old, f->port.program_size, off + pos))
            return NX_STORAGE_IO;
        for (size_t i = 0; i < f->port.program_size; ++i) {
            uint8_t next = ((const uint8_t*)in)[pos + i];
            if ((old[i] & next) != next) return NX_STORAGE_INVALID;
        }
    }
    return mutate(f, off, in, n);
}
static nx_storage_status_t file_erase(void* ctx, size_t off, size_t n) {
    nx_file_flash_t* f = ctx;
    if (!range(f, off, n) || off % f->port.erase_size || n % f->port.erase_size)
        return NX_STORAGE_INVALID;
    uint8_t erased[4096]; memset(erased, 0xff, sizeof(erased));
    for (size_t pos = 0; pos < n;) {
        size_t count = n - pos;
        if (count > sizeof(erased)) count = sizeof(erased);
        nx_storage_status_t status = mutate(f, off + pos, erased, count);
        if (status != NX_STORAGE_OK) return status;
        pos += count;
    }
    return NX_STORAGE_OK;
}
static nx_storage_status_t file_sync(void* ctx) {
    nx_file_flash_t* f = ctx;
    if (!f || f->fd < 0 || !event(f)) return NX_STORAGE_IO;
    return fsync(f->fd) == 0 ? NX_STORAGE_OK : NX_STORAGE_IO;
}
void nx_file_flash_fail_after(nx_file_flash_t* f, int64_t events) {
    if (f) { f->fail_after = events; f->mutation_events = 0; }
}
void nx_file_flash_close(nx_file_flash_t* f) {
    if (f && f->fd >= 0) { close(f->fd); f->fd = -1; }
}
nx_storage_status_t nx_file_flash_open(nx_file_flash_t* f, const char* path,
                                       size_t size, size_t erase_size,
                                       size_t program_size) {
    if (!f || !path || !size || !erase_size || !program_size ||
        program_size > NX_STORAGE_MAX_PROGRAM_SIZE ||
        erase_size % program_size || size % erase_size || size > INT64_MAX)
        return NX_STORAGE_INVALID;
    memset(f, 0, sizeof(*f)); f->fd = -1; f->fail_after = -1;
    int flags = O_RDWR | O_CLOEXEC;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    bool created = false;
    f->fd = open(path, flags | O_CREAT | O_EXCL, 0600);
    if (f->fd >= 0) created = true;
    else if (errno == EEXIST) f->fd = open(path, flags);
    if (f->fd < 0 || flock(f->fd, LOCK_EX | LOCK_NB) != 0) {
        nx_file_flash_close(f); return NX_STORAGE_IO;
    }
    struct stat st;
    if (fstat(f->fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        nx_file_flash_close(f); return NX_STORAGE_INVALID;
    }
    if (created) {
        uint8_t erased[4096]; memset(erased, 0xff, sizeof(erased));
        for (size_t pos = 0; pos < size;) {
            size_t count = size - pos;
            if (count > sizeof(erased)) count = sizeof(erased);
            if (!write_full(f->fd, erased, count, pos)) {
                nx_file_flash_close(f); return NX_STORAGE_IO;
            }
            pos += count;
        }
        if (fsync(f->fd) != 0) { nx_file_flash_close(f); return NX_STORAGE_IO; }
    } else if (st.st_size < 0 || (uint64_t)st.st_size != size) {
        nx_file_flash_close(f); return NX_STORAGE_INVALID;
    }
    f->port = (nx_flash_port_t){f, size, erase_size, program_size,
                                file_read, file_program, file_erase, file_sync};
    return NX_STORAGE_OK;
}

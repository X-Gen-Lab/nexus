#include "nexus/storage.h"
#include <limits.h>
#include <string.h>

/* Wire format v1: LE magic/schema/length/generation/payload CRC/header CRC,
 * reserved zero bytes. Header is rounded to a whole program unit; a separate
 * full program unit is committed last. Padding is erased 0xff. CRC is detection
 * only; authentication belongs to the security provider. */
static uint32_t crc_update(uint32_t crc, const uint8_t* p, size_t n) {
    while (n--) {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}
static uint32_t crc(const void* p, size_t n) {
    return ~crc_update(UINT32_MAX, (const uint8_t*)p, n);
}
static void put32(uint8_t* p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
static uint32_t get32(const uint8_t* p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8u * i);
    return v;
}
static void put64(uint8_t* p, uint64_t v) {
    put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32));
}
static uint64_t get64(const uint8_t* p) {
    return get32(p) | ((uint64_t)get32(p + 4) << 32);
}
static size_t round_up(size_t n, size_t alignment) {
    return ((n + alignment - 1) / alignment) * alignment;
}
static size_t bank_offset(const nx_storage_t* s, int bank) {
    return s->offset + (size_t)bank * s->bank_size;
}
static nx_storage_status_t read_bank(nx_storage_t* s, int bank,
                                     uint64_t* generation, size_t* size,
                                     bool* committed) {
    uint8_t b[NX_STORAGE_MAX_PROGRAM_SIZE];
    size_t base = bank_offset(s, bank);
    size_t marker = s->payload_offset - s->flash.program_size;
    nx_storage_status_t status = s->flash.read(s->flash.ctx, base + marker, b,
                                              s->flash.program_size);
    if (status != NX_STORAGE_OK) return status;
    *committed = true;
    for (size_t i = 0; i < s->flash.program_size; ++i)
        if (b[i] != 0) { *committed = false; return NX_STORAGE_NOT_FOUND; }
    status = s->flash.read(s->flash.ctx, base, b, NX_STORAGE_HEADER_SIZE);
    if (status != NX_STORAGE_OK) return status;
    if (memcmp(b, "NXST", 4) || get32(b + 4) != 1 ||
        get32(b + 24) != crc(b, 24) || get32(b + 28) != 0)
        return NX_STORAGE_CORRUPT;
    *size = get32(b + 8);
    *generation = get64(b + 12);
    if (*size > nx_storage_capacity(s) || *generation == 0)
        return NX_STORAGE_CORRUPT;
    uint32_t expected = get32(b + 20), actual = UINT32_MAX;
    for (size_t n = 0; n < *size;) {
        size_t count = *size - n;
        if (count > sizeof(b)) count = sizeof(b);
        status = s->flash.read(s->flash.ctx, base + s->payload_offset + n, b,
                               count);
        if (status != NX_STORAGE_OK) return status;
        actual = crc_update(actual, b, count);
        n += count;
    }
    return expected == ~actual ? NX_STORAGE_OK : NX_STORAGE_CORRUPT;
}

nx_storage_status_t nx_storage_open(nx_storage_t* s, const nx_flash_port_t* p,
                                    size_t offset, size_t bank_size) {
    if (!s || !p || !p->read || !p->program || !p->erase || !p->sync ||
        !p->program_size || !p->erase_size ||
        p->program_size > NX_STORAGE_MAX_PROGRAM_SIZE ||
        p->erase_size % p->program_size || offset % p->erase_size ||
        bank_size % p->erase_size || offset > p->size ||
        bank_size > (p->size - offset) / 2 ||
        bank_size <= round_up(NX_STORAGE_HEADER_SIZE, p->program_size) +
                         p->program_size)
        return NX_STORAGE_INVALID;
    /* Reopening after an I/O error may borrow the instance's existing port.
     * Preserve it before resetting the destination, including its geometry. */
    nx_flash_port_t port = *p;
    memset(s, 0, sizeof(*s));
    s->flash = port; s->offset = offset; s->bank_size = bank_size;
    s->payload_offset = round_up(NX_STORAGE_HEADER_SIZE, port.program_size) +
                        port.program_size;
    s->active_bank = -1;
    bool saw_corrupt = false;
    for (int bank = 0; bank < 2; ++bank) {
        uint64_t gen = 0; size_t len = 0; bool committed = false;
        nx_storage_status_t status = read_bank(s, bank, &gen, &len, &committed);
        if (status == NX_STORAGE_IO) return status;
        if (status != NX_STORAGE_OK && status != NX_STORAGE_NOT_FOUND &&
            status != NX_STORAGE_CORRUPT) return status;
        if (status == NX_STORAGE_CORRUPT && committed) saw_corrupt = true;
        if (status == NX_STORAGE_OK) {
            if (s->active_bank >= 0 && gen == s->generation)
                return NX_STORAGE_CORRUPT;
            if (s->active_bank < 0 || gen > s->generation) {
                s->active_bank = bank; s->generation = gen; s->payload_size = len;
            }
        }
    }
    if (s->active_bank < 0 && saw_corrupt) return NX_STORAGE_CORRUPT;
    s->opened = true;
    return NX_STORAGE_OK;
}

size_t nx_storage_capacity(const nx_storage_t* s) {
    return s && s->bank_size >= s->payload_offset ?
               s->bank_size - s->payload_offset : 0;
}
nx_storage_status_t nx_storage_load(nx_storage_t* s, void* out, size_t* size) {
    if (!s || !s->opened || !size) return NX_STORAGE_INVALID;
    if (s->active_bank < 0) return NX_STORAGE_NOT_FOUND;
    if (!out) { *size = s->payload_size; return NX_STORAGE_OK; }
    if (*size < s->payload_size) {
        *size = s->payload_size; return NX_STORAGE_NO_SPACE;
    }
    /* Revalidate before exposing a corrupted payload after post-open damage. */
    uint64_t gen; size_t len; bool committed;
    nx_storage_status_t status = read_bank(s, s->active_bank, &gen, &len,
                                           &committed);
    if (status != NX_STORAGE_OK) return status;
    if (gen != s->generation || len != s->payload_size)
        return NX_STORAGE_CORRUPT; /* Partition identity changed after open. */
    status = s->flash.read(s->flash.ctx,
                           bank_offset(s, s->active_bank) + s->payload_offset,
                           out, s->payload_size);
    if (status == NX_STORAGE_OK) *size = s->payload_size;
    return status;
}

nx_storage_status_t nx_storage_save(nx_storage_t* s, const void* data,
                                    size_t size) {
    if (!s || !s->opened || (!data && size)) return NX_STORAGE_INVALID;
    if (size > nx_storage_capacity(s) || size > UINT32_MAX)
        return NX_STORAGE_NO_SPACE;
    if (s->generation == UINT64_MAX) return NX_STORAGE_NO_SPACE;
    int bank = s->active_bank == 0 ? 1 : 0;
    size_t base = bank_offset(s, bank), unit = s->flash.program_size;
    uint8_t block[NX_STORAGE_MAX_PROGRAM_SIZE], header[NX_STORAGE_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    memcpy(header, "NXST", 4); put32(header + 4, 1);
    put32(header + 8, (uint32_t)size); put64(header + 12, s->generation + 1);
    put32(header + 20, crc(data, size)); put32(header + 24, crc(header, 24));
    nx_storage_status_t status = s->flash.erase(s->flash.ctx, base, s->bank_size);
    if (status != NX_STORAGE_OK) { s->opened = false; return status; }
    size_t header_space = s->payload_offset - unit;
    for (size_t pos = 0; pos < header_space; pos += unit) {
        memset(block, 0xff, unit);
        size_t count = pos < sizeof(header) ? sizeof(header) - pos : 0;
        if (count > unit) count = unit;
        if (count) memcpy(block, header + pos, count);
        status = s->flash.program(s->flash.ctx, base + pos, block, unit);
        if (status != NX_STORAGE_OK) { s->opened = false; return status; }
    }
    for (size_t pos = 0; pos < size; pos += unit) {
        size_t count = size - pos;
        if (count > unit) count = unit;
        memset(block, 0xff, unit);
        memcpy(block, (const uint8_t*)data + pos, count);
        status = s->flash.program(s->flash.ctx,
                                  base + s->payload_offset + pos, block, unit);
        if (status != NX_STORAGE_OK) { s->opened = false; return status; }
    }
    status = s->flash.sync(s->flash.ctx);
    if (status != NX_STORAGE_OK) { s->opened = false; return status; }
    memset(block, 0, unit);
    status = s->flash.program(s->flash.ctx, base + header_space, block, unit);
    if (status == NX_STORAGE_OK) status = s->flash.sync(s->flash.ctx);
    if (status != NX_STORAGE_OK) {
        /* The commit could be complete. Caller must reopen this instance. */
        s->opened = false;
        return status;
    }
    s->active_bank = bank; ++s->generation; s->payload_size = size;
    return NX_STORAGE_OK;
}

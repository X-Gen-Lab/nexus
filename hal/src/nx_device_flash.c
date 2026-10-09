/** Checked physical flash geometry and external, caller-selected region leases. */
#include "nx_device_internal.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <string.h>
_Static_assert(NX_DEVICE_FLASH_MAX_REGIONS > 0 && NX_DEVICE_FLASH_MAX_REGIONS <= UINT32_MAX,
               "Flash region capacity must be representable");
typedef struct {
    nx_device_ref_t controller;
    uint64_t generation;
    uint32_t offset, size, permissions;
    bool allocated, running;
    uint32_t borrowers;
} region_t;
static region_t regions[NX_DEVICE_FLASH_MAX_REGIONS];
typedef struct {
    nx_device_flash_region_t region;
    uint64_t generation;
    bool allocated;
} borrow_t;
static borrow_t borrows[NX_DEVICE_FLASH_MAX_REGIONS];
static uint32_t enter(void) { return nx_arch_irq_save().value; }
static void leave(uint32_t s) { nx_arch_irq_restore((nx_arch_irq_state_t){s}); }
static bool same(nx_device_ref_t a, nx_device_ref_t b) {
    return a.descriptor == b.descriptor && a.owner == b.owner &&
        a.generation == b.generation && a.device_class == b.device_class;
}
static region_t* resolve(nx_device_flash_region_t ref) {
    if (!ref.slot || ref.slot > NX_DEVICE_FLASH_MAX_REGIONS || !ref.generation) return NULL;
    region_t* r = &regions[ref.slot - 1];
    return r->allocated && r->generation == ref.generation && same(r->controller, ref.controller) ? r : NULL;
}
static nx_status_t pin(nx_device_ref_t ref, bool serialize, nx_internal_flash_t** flash) {
    void* api = NULL;
    nx_status_t s = nx_device_dispatch_pin(ref, NX_DEVICE_CLASS_FLASH, serialize, &api);
    if (s == NX_OK) *flash = api;
    return s;
}
static nx_flash_operations_t* ops(nx_internal_flash_t* flash) {
    return flash->get_operations ? flash->get_operations(flash) : NULL;
}
static nx_status_t geometry(nx_flash_operations_t* port, nx_flash_geometry_t* out) {
    if (!port || !port->get_geometry || !port->get_block || !port->read) return NX_ERR_NOT_SUPPORTED;
    nx_status_t s = port->get_geometry(port, out);
    if (s == NX_OK && (!out->size_bytes || !out->block_count || !out->program_alignment ||
        (out->program_alignment & (out->program_alignment - 1)) ||
        out->size_bytes % out->program_alignment ||
        out->base_address > UINTPTR_MAX - out->size_bytes)) s = NX_ERR_INVALID_STATE;
    return s;
}
static nx_status_t block(nx_flash_operations_t* port, const nx_flash_geometry_t* g,
                         uint32_t offset, nx_flash_block_t* out) {
    if (offset >= g->size_bytes) return NX_ERR_INVALID_PARAM;
    nx_status_t s = port->get_block(port, offset, out);
    if (s == NX_OK && (!out->size || out->index >= g->block_count ||
        out->offset > offset || out->offset >= g->size_bytes ||
        out->size > g->size_bytes - out->offset || offset - out->offset >= out->size))
        s = NX_ERR_INVALID_STATE;
    return s;
}
static nx_status_t erase_range(nx_flash_operations_t* port, const nx_flash_geometry_t* g,
                              uint32_t offset, size_t len) {
    if (!len || offset >= g->size_bytes || len > g->size_bytes - offset) return NX_ERR_INVALID_PARAM;
    uint32_t end = offset + (uint32_t)len;
    for (uint32_t count = 0; offset < end && count < g->block_count; ++count) {
        nx_flash_block_t b = {0};
        nx_status_t s = block(port, g, offset, &b);
        if (s != NX_OK) return s;
        if (b.offset != offset || b.size > end - offset) return NX_ERR_INVALID_PARAM;
        offset += b.size;
    }
    return offset == end ? NX_OK : NX_ERR_INVALID_STATE;
}
nx_status_t nx_device_flash_geometry(nx_device_ref_t ref, nx_flash_geometry_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref, true, &f);
    if (s == NX_OK) { s = geometry(ops(f), out); nx_device_dispatch_unpin(ref); }
    return s;
}
nx_status_t nx_device_flash_block(nx_device_ref_t ref, uint32_t offset, nx_flash_block_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref, true, &f);
    if (s == NX_OK) {
        nx_flash_geometry_t g = {0}; nx_flash_operations_t* p = ops(f);
        s = geometry(p, &g); if (s == NX_OK) s = block(p, &g, offset, out);
        nx_device_dispatch_unpin(ref);
    }
    return s;
}
nx_status_t nx_device_flash_set_write_enabled(nx_device_ref_t ref, bool enabled) {
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref, true, &f);
    if (s == NX_OK) {
        s = enabled ? (f->unlock ? f->unlock(f) : NX_ERR_NOT_SUPPORTED) :
                      (f->lock ? f->lock(f) : NX_ERR_NOT_SUPPORTED);
        nx_device_dispatch_unpin(ref);
    }
    return s;
}
nx_status_t nx_device_flash_region_open(nx_device_ref_t ref, uint32_t offset, uint32_t size,
                                       uint32_t permissions, nx_device_flash_region_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    const uint32_t all = NX_FLASH_REGION_READ | NX_FLASH_REGION_PROGRAM | NX_FLASH_REGION_ERASE;
    if (!size || !permissions || (permissions & ~all)) return NX_ERR_INVALID_PARAM;
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref, true, &f);
    if (s != NX_OK) return s;
    nx_flash_geometry_t g = {0}; nx_flash_operations_t* p = ops(f);
    s = geometry(p, &g);
    if (s == NX_OK && (offset >= g.size_bytes || size > g.size_bytes - offset)) s = NX_ERR_INVALID_PARAM;
    if (s == NX_OK && (permissions & NX_FLASH_REGION_PROGRAM) &&
        (offset % g.program_alignment || size % g.program_alignment)) s = NX_ERR_INVALID_PARAM;
    if (s == NX_OK && (permissions & NX_FLASH_REGION_PROGRAM) && !p->program) s = NX_ERR_NOT_SUPPORTED;
    if (s == NX_OK && (permissions & NX_FLASH_REGION_ERASE)) {
        s = p->erase ? erase_range(p, &g, offset, size) : NX_ERR_NOT_SUPPORTED;
    }
    if (s == NX_OK) {
        uint32_t saved = enter(); region_t* available = NULL; uint32_t slot = 0;
        const uint32_t write = NX_FLASH_REGION_PROGRAM | NX_FLASH_REGION_ERASE;
        for (uint32_t i = 0; i < NX_DEVICE_FLASH_MAX_REGIONS; ++i) {
            region_t* r = &regions[i];
            if (r->allocated && same(r->controller, ref) && ((r->permissions | permissions) & write) &&
                offset < r->offset + r->size && r->offset < offset + size) { s = NX_ERR_RESOURCE_BUSY; break; }
            if (!r->allocated && r->generation != UINT64_MAX && !available) { available = r; slot = i + 1; }
        }
        if (s == NX_OK && (!available || ref.descriptor->state->child_refs == UINT32_MAX)) s = NX_ERR_NO_RESOURCE;
        if (s == NX_OK) {
            uint64_t next = available->generation + 1;
            *available = (region_t){ref, next, offset, size, permissions, true, false, 0};
            ++ref.descriptor->state->child_refs;
            *out = (nx_device_flash_region_t){ref, next, slot};
        }
        leave(saved);
    }
    nx_device_dispatch_unpin(ref); return s;
}
nx_status_t nx_device_flash_region_close(nx_device_flash_region_t ref) {
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref.controller, false, &f);
    if (s != NX_OK) return s;
    uint32_t saved = enter(); region_t* r = resolve(ref);
    s = !r ? NX_ERR_INVALID_STATE : r->running || r->borrowers ? NX_ERR_BUSY : NX_OK;
    if (s == NX_OK) { r->allocated = false; --ref.controller.descriptor->state->child_refs; }
    leave(saved); nx_device_dispatch_unpin(ref.controller); return s;
}
nx_status_t nx_device_flash_region_info(nx_device_flash_region_t ref,
                                       nx_flash_region_info_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref.controller, false, &f);
    if (s != NX_OK) return s;
    uint32_t saved = enter(); region_t* r = resolve(ref);
    s = r ? NX_OK : NX_ERR_INVALID_STATE;
    if (r) *out = (nx_flash_region_info_t){r->offset, r->size, r->permissions};
    leave(saved); nx_device_dispatch_unpin(ref.controller); return s;
}
nx_status_t nx_device_flash_region_borrow(nx_device_flash_region_t ref,
                                         nx_device_flash_borrow_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref.controller, false, &f);
    if (s != NX_OK) return s;
    uint32_t saved = enter(); region_t* r = resolve(ref); borrow_t* loan = NULL; uint32_t slot = 0;
    s = !r ? NX_ERR_INVALID_STATE : r->borrowers == UINT32_MAX ? NX_ERR_NO_RESOURCE : NX_OK;
    if (s == NX_OK) {
        for (uint32_t i = 0; i < NX_DEVICE_FLASH_MAX_REGIONS; ++i) {
            if (!borrows[i].allocated && borrows[i].generation != UINT64_MAX) {
                loan = &borrows[i]; slot = i + 1; break;
            }
        }
        if (!loan) s = NX_ERR_NO_RESOURCE;
        else {
            ++loan->generation; loan->allocated = true; loan->region = ref;
            ++r->borrowers;
            *out = (nx_device_flash_borrow_t){ref, loan->generation, slot};
        }
    }
    leave(saved); nx_device_dispatch_unpin(ref.controller); return s;
}
nx_status_t nx_device_flash_region_release(nx_device_flash_borrow_t ref) {
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref.region.controller, false, &f);
    if (s != NX_OK) return s;
    uint32_t saved = enter(); region_t* r = resolve(ref.region); borrow_t* loan = NULL;
    if (ref.slot && ref.slot <= NX_DEVICE_FLASH_MAX_REGIONS && ref.generation)
        loan = &borrows[ref.slot - 1];
    s = !r || !loan || !loan->allocated || loan->generation != ref.generation ||
        loan->region.slot != ref.region.slot || loan->region.generation != ref.region.generation ||
        !same(loan->region.controller, ref.region.controller) ? NX_ERR_INVALID_STATE : NX_OK;
    if (s == NX_OK) { loan->allocated = false; --r->borrowers; }
    leave(saved); nx_device_dispatch_unpin(ref.region.controller); return s;
}
static nx_status_t action(nx_device_flash_region_t ref, uint32_t offset, void* data,
                          size_t len, uint32_t budget, uint32_t permission) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (!len || (!data && permission != NX_FLASH_REGION_ERASE) || budget > INT32_MAX)
        return NX_ERR_INVALID_PARAM;
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref.controller, true, &f);
    if (s != NX_OK) return s;
    uint32_t saved = enter(); region_t* r = resolve(ref); uint32_t physical = 0;
    s = !r ? NX_ERR_INVALID_STATE : r->running ? NX_ERR_BUSY :
        !(r->permissions & permission) ? NX_ERR_PERMISSION :
        offset >= r->size || len > r->size - offset ? NX_ERR_INVALID_PARAM : NX_OK;
    if (s == NX_OK) { r->running = true; physical = r->offset + offset; }
    leave(saved);
    if (s == NX_OK) {
        nx_flash_operations_t* p = ops(f); nx_flash_geometry_t g = {0};
        s = geometry(p, &g);
        if (s == NX_OK && (physical >= g.size_bytes || len > g.size_bytes - physical))
            s = NX_ERR_INVALID_STATE;
        if (s == NX_OK && permission != NX_FLASH_REGION_READ && !budget) s = NX_ERR_TIMEOUT;
        if (s == NX_OK && permission == NX_FLASH_REGION_PROGRAM &&
            (physical % g.program_alignment || len % g.program_alignment)) s = NX_ERR_INVALID_PARAM;
        if (s == NX_OK && permission == NX_FLASH_REGION_ERASE) s = erase_range(p, &g, physical, len);
        if (s == NX_OK) {
            if (permission == NX_FLASH_REGION_READ) s = p->read(p, physical, data, len);
            else if (permission == NX_FLASH_REGION_PROGRAM) s = p->program ? p->program(p, physical, data, len, budget) : NX_ERR_NOT_SUPPORTED;
            else s = p->erase ? p->erase(p, physical, len, budget) : NX_ERR_NOT_SUPPORTED;
        }
        saved = enter(); r->running = false; leave(saved);
    }
    nx_device_dispatch_unpin(ref.controller); return s;
}
nx_status_t nx_device_flash_read(nx_device_flash_region_t ref, uint32_t offset, uint8_t* data, size_t len) {
    return action(ref, offset, data, len, 0, NX_FLASH_REGION_READ);
}
nx_status_t nx_device_flash_program(nx_device_flash_region_t ref, uint32_t offset, const uint8_t* data,
                                    size_t len, uint32_t budget) {
    return action(ref, offset, (void*)data, len, budget, NX_FLASH_REGION_PROGRAM);
}
nx_status_t nx_device_flash_erase(nx_device_flash_region_t ref, uint32_t offset, size_t len, uint32_t budget) {
    return action(ref, offset, NULL, len, budget, NX_FLASH_REGION_ERASE);
}
nx_status_t nx_device_flash_sync(nx_device_ref_t ref, uint32_t budget) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (budget > INT32_MAX) return NX_ERR_INVALID_PARAM;
    nx_internal_flash_t* f = NULL; nx_status_t s = pin(ref, true, &f);
    if (s == NX_OK) {
        nx_flash_operations_t* p = ops(f);
        s = !budget ? NX_ERR_TIMEOUT : !p || !p->sync ? NX_ERR_NOT_SUPPORTED : p->sync(p, budget);
        nx_device_dispatch_unpin(ref);
    }
    return s;
}

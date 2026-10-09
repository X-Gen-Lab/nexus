#include "nexus/hal_flash_storage.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <string.h>
_Static_assert(NX_HAL_FLASH_STORAGE_MAX_BINDINGS > 0,
               "Flash adapter needs a bounded binding capacity");
typedef struct {
    nx_hal_flash_storage_t* context;
    uintptr_t token;
    bool pinned;
} binding_t;
static binding_t bindings[NX_HAL_FLASH_STORAGE_MAX_BINDINGS];
static uintptr_t next_token;
static uint32_t enter(void) { return nx_arch_irq_save().value; }
static void leave(uint32_t s) { nx_arch_irq_restore((nx_arch_irq_state_t){s}); }
static nx_status_t task(void) {
    return nx_arch_in_isr() ? NX_ERR_CONTEXT : nx_arch_irq_is_masked() ? NX_ERR_INVALID_STATE : NX_OK;
}
static nx_status_t pin_context(const nx_hal_flash_storage_t* ctx, uintptr_t token,
                                binding_t** out) {
    nx_status_t s = task(); if (s != NX_OK) return s;
    uint32_t saved = enter(); binding_t* found = NULL;
    for (size_t i=0; i<NX_HAL_FLASH_STORAGE_MAX_BINDINGS; ++i) {
        if (bindings[i].context && (ctx ? bindings[i].context == ctx : token && bindings[i].token == token)) {
            found = &bindings[i]; break;
        }
    }
    s = !found ? NX_ERR_INVALID_STATE : found->pinned ? NX_ERR_BUSY : NX_OK;
    if (s == NX_OK) { found->pinned = true; *out = found; }
    leave(saved); return s;
}
static void unpin(binding_t* b) {
    uint32_t saved = enter(); b->pinned = false; leave(saved);
}
static nx_status_t remaining(nx_hal_flash_storage_t* ctx, uint32_t* out) {
    *out = 0;
    if (!ctx->active) return NX_ERR_INVALID_STATE;
    uint32_t now = 0; nx_status_t s = ctx->clock.now_ms(ctx->clock.context, &now);
    if (s == NX_OK) {
        uint32_t elapsed = now - ctx->started_ms;
        if (elapsed >= ctx->budget_ms) return NX_ERR_TIMEOUT;
        *out = ctx->budget_ms - elapsed;
    }
    return s;
}
static nx_storage_status_t coarse(nx_status_t s) {
    if (s == NX_OK) return NX_STORAGE_OK;
    if (s == NX_ERR_NOT_SUPPORTED) return NX_STORAGE_UNSUPPORTED;
    if (s == NX_ERR_INVALID_PARAM || s == NX_ERR_INVALID_STATE || s == NX_ERR_NOT_FOUND || s == NX_ERR_NULL_PTR)
        return NX_STORAGE_INVALID;
    return NX_STORAGE_IO;
}
static nx_storage_status_t io(void* token, size_t offset, void* data, size_t size, unsigned action) {
    binding_t* b = NULL; nx_status_t s = pin_context(NULL, (uintptr_t)token, &b);
    if (s != NX_OK) return coarse(s);
    nx_hal_flash_storage_t* ctx = b->context; uint32_t left = 0;
    s = remaining(ctx, &left);
    if (s == NX_OK && offset > UINT32_MAX) s = NX_ERR_INVALID_PARAM;
    if (s == NX_OK) {
        if (action == 0 && !size) {
            nx_flash_region_info_t info;
            s = nx_device_flash_region_info(ctx->loan.region, &info);
            if (s == NX_OK && offset > info.size) s = NX_ERR_INVALID_PARAM;
        } else if (action == 0) s = nx_device_flash_read(ctx->loan.region, (uint32_t)offset, data, size);
        else if (action == 1) s = nx_device_flash_program(ctx->loan.region, (uint32_t)offset, data, size, left);
        else if (action == 2) s = nx_device_flash_erase(ctx->loan.region, (uint32_t)offset, size, left);
        else s = nx_device_flash_sync(ctx->loan.region.controller, left);
    }
    if (s == NX_OK) s = remaining(ctx, &left);
    ctx->last_status = s;
    unpin(b); return coarse(s);
}
static nx_storage_status_t read(void* ctx, size_t offset, void* data, size_t size) {
    return io(ctx, offset, data, size, 0);
}
static nx_storage_status_t program(void* ctx, size_t offset, const void* data, size_t size) {
    return io(ctx, offset, (void*)data, size, 1);
}
static nx_storage_status_t erase(void* ctx, size_t offset, size_t size) {
    return io(ctx, offset, NULL, size, 2);
}
static nx_storage_status_t sync(void* ctx) { return io(ctx, 0, NULL, 0, 3); }
static nx_status_t validate(nx_hal_flash_storage_t* ctx, nx_flash_port_t* port) {
    nx_flash_region_info_t info; nx_flash_geometry_t g;
    uint32_t left = 0; nx_status_t s = remaining(ctx, &left);
    if (s == NX_OK) s = nx_device_flash_region_info(ctx->loan.region, &info);
    const uint32_t all = NX_FLASH_REGION_READ | NX_FLASH_REGION_PROGRAM | NX_FLASH_REGION_ERASE;
    if (s == NX_OK && (info.permissions & all) != all) s = NX_ERR_PERMISSION;
    if (s == NX_OK) s = remaining(ctx, &left);
    if (s == NX_OK) s = nx_device_flash_geometry(ctx->loan.region.controller, &g);
    if (s == NX_OK && (g.erased_value != 0xff ||
        g.program_alignment > NX_STORAGE_MAX_PROGRAM_SIZE ||
        info.offset >= g.size_bytes || info.size > g.size_bytes - info.offset)) s = NX_ERR_NOT_SUPPORTED;
    uint32_t at = s == NX_OK ? info.offset : 0; uint32_t unit = 0;
    uint32_t end = s == NX_OK ? info.offset + info.size : 0;
    while (s == NX_OK && at < end) {
        s = remaining(ctx, &left); if (s != NX_OK) break;
        nx_flash_block_t block; s = nx_device_flash_block(ctx->loan.region.controller, at, &block);
        if (s != NX_OK) break;
        if (block.offset != at || block.size > end - at || (unit && block.size != unit)) {
            s = NX_ERR_NOT_SUPPORTED; break;
        }
        unit = block.size; at += block.size;
    }
    if (s == NX_OK) s = remaining(ctx, &left);
    if (s == NX_OK && (!unit || info.size / unit < 2 || info.size % unit || unit % g.program_alignment))
        s = NX_ERR_NOT_SUPPORTED;
    if (s == NX_OK) {
        port->size = info.size; port->erase_size = unit; port->program_size = g.program_alignment;
        port->read = read; port->program = program; port->erase = erase; port->sync = sync;
    }
    return s;
}
nx_status_t nx_hal_flash_storage_bind(nx_hal_flash_storage_t* ctx,
                                      nx_device_flash_region_t region,
                                      const nx_hal_flash_storage_clock_t* clock,
                                      uint32_t budget, nx_flash_port_t* out) {
    if (!ctx || !clock || !out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    if (!clock->now_ms || !budget || budget > INT32_MAX) return NX_ERR_INVALID_PARAM;
    const nx_hal_flash_storage_clock_t clock_copy = *clock;
    nx_status_t s = task(); if (s != NX_OK) return s;
    uint32_t saved = enter(); binding_t* b = NULL;
    for (size_t i=0; i<NX_HAL_FLASH_STORAGE_MAX_BINDINGS; ++i) {
        if (bindings[i].context == ctx) { leave(saved); return NX_ERR_BUSY; }
        if (!bindings[i].context && !b) b = &bindings[i];
    }
    if (!b || next_token == UINTPTR_MAX) { leave(saved); return NX_ERR_NO_RESOURCE; }
    *b = (binding_t){ctx, ++next_token, true};
    memset(ctx, 0, sizeof(*ctx)); ctx->clock = clock_copy;
    leave(saved);
    s = clock_copy.now_ms(clock_copy.context, &ctx->started_ms);
    if (s == NX_OK) { ctx->active = true; ctx->budget_ms = budget; }
    if (s == NX_OK) s = nx_device_flash_region_borrow(region, &ctx->loan);
    nx_flash_port_t port = {0};
    if (s == NX_OK) s = validate(ctx, &port);
    ctx->active = false; ctx->last_status = s;
    if (s == NX_OK) { port.ctx = (void*)b->token; *out = port; unpin(b); return NX_OK; }
    nx_status_t cleanup = ctx->loan.slot ? nx_device_flash_region_release(ctx->loan) : NX_OK;
    if (cleanup != NX_OK) { ctx->last_status = cleanup; unpin(b); return cleanup; }
    saved = enter(); *b = (binding_t){0}; memset(ctx, 0, sizeof(*ctx)); leave(saved);
    return s;
}
nx_status_t nx_hal_flash_storage_begin(nx_hal_flash_storage_t* ctx, uint32_t budget) {
    if (!ctx) return NX_ERR_NULL_PTR;
    if (!budget || budget > INT32_MAX) return NX_ERR_INVALID_PARAM;
    binding_t* b = NULL; nx_status_t s = pin_context(ctx, 0, &b); if (s != NX_OK) return s;
    if (ctx->active) s = NX_ERR_BUSY;
    else {
        s = ctx->clock.now_ms(ctx->clock.context, &ctx->started_ms);
        if (s == NX_OK) { ctx->active = true; ctx->budget_ms = budget; }
    }
    ctx->last_status = s; unpin(b); return s;
}
nx_status_t nx_hal_flash_storage_end(nx_hal_flash_storage_t* ctx) {
    if (!ctx) return NX_ERR_NULL_PTR;
    binding_t* b = NULL; nx_status_t s = pin_context(ctx, 0, &b); if (s != NX_OK) return s;
    s = ctx->active ? NX_OK : NX_ERR_INVALID_STATE;
    if (s == NX_OK) ctx->active = false;
    unpin(b); return s;
}
nx_status_t nx_hal_flash_storage_unbind(nx_hal_flash_storage_t* ctx) {
    if (!ctx) return NX_ERR_NULL_PTR;
    binding_t* b = NULL; nx_status_t s = pin_context(ctx, 0, &b); if (s != NX_OK) return s;
    if (ctx->active) { unpin(b); return NX_ERR_BUSY; }
    s = nx_device_flash_region_release(ctx->loan);
    if (s != NX_OK) { ctx->last_status = s; unpin(b); return s; }
    uint32_t saved = enter(); memset(ctx, 0, sizeof(*ctx)); *b = (binding_t){0}; leave(saved);
    return NX_OK;
}
nx_status_t nx_hal_flash_storage_last_status(const nx_hal_flash_storage_t* ctx) {
    if (!ctx) return NX_ERR_NULL_PTR;
    binding_t* b = NULL; nx_status_t s = pin_context(ctx, 0, &b); if (s != NX_OK) return s;
    s = ctx->last_status; unpin(b); return s;
}

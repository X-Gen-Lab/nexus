/** Typed SPI children and completion leases. No OSAL, heap or SDK dependency. */
#include "nx_device_internal.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <string.h>

_Static_assert(NX_DEVICE_SPI_MAX_CHILDREN > 0 && NX_DEVICE_SPI_MAX_CHILDREN <= UINT32_MAX,
               "SPI child pool must have a nonzero representable capacity");
typedef enum { SPI_FREE, SPI_OPENING, SPI_READY, SPI_CLOSING, SPI_QUARANTINED } spi_phase_t;
typedef struct {
    nx_device_ref_t controller;
    nx_spi_device_t driver;
    uint64_t generation, next_ticket, last_ticket, active_ticket;
    spi_phase_t phase;
    bool running, submitting, cancelling, callback_active, settled;
    nx_status_t terminal_status;
    nx_spi_terminal_callback_t callback;
    void* user_data;
} spi_child_t;
static spi_child_t children[NX_DEVICE_SPI_MAX_CHILDREN];

static uint32_t enter(void) { return nx_arch_irq_save().value; }
static void leave(uint32_t saved) { nx_arch_irq_restore((nx_arch_irq_state_t){saved}); }
static bool same_controller(nx_device_ref_t a, nx_device_ref_t b) {
    return a.descriptor == b.descriptor && a.owner == b.owner &&
           a.generation == b.generation && a.device_class == b.device_class;
}
/* Called only with the controller pinned and metadata mask held. */
static spi_child_t* resolve(nx_device_spi_ref_t ref) {
    if (!ref.slot || ref.slot > NX_DEVICE_SPI_MAX_CHILDREN || !ref.generation) return NULL;
    spi_child_t* child = &children[ref.slot - 1u];
    return child->phase == SPI_READY && child->generation == ref.generation &&
           same_controller(child->controller, ref.controller) ? child : NULL;
}
static bool busy(const spi_child_t* child) {
    return child->running || child->submitting || child->cancelling ||
           child->callback_active || child->active_ticket;
}
static nx_status_t parent_pin(nx_device_ref_t ref, nx_spi_bus_t** bus, bool serialize) {
    void* api = NULL;
    nx_status_t status = nx_device_dispatch_pin(ref, NX_DEVICE_CLASS_SPI, serialize, &api);
    if (status == NX_OK) *bus = api;
    return status;
}
static bool transaction_valid(const nx_spi_transaction_t* t, bool async) {
    return t && t->tx_data && t->length && t->length <= UINT16_MAX &&
           (t->timeout_ms <= INT32_MAX || (!async && t->timeout_ms == UINT32_MAX)) &&
           (!async || t->timeout_ms);
}

nx_status_t nx_device_spi_open(nx_device_ref_t controller,
                              const nx_spi_device_config_t* config,
                              nx_device_spi_ref_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    if (!config || !config->speed || config->mode > NX_SPI_MODE_3 ||
        config->bit_order > NX_SPI_BIT_ORDER_LSB) return NX_ERR_INVALID_PARAM;
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(controller, &bus, true);
    if (status != NX_OK) return status;
    if (!bus->open_device || !bus->close_device) {
        nx_device_dispatch_unpin(controller);
        return NX_ERR_NOT_SUPPORTED;
    }
    uint32_t saved = enter();
    spi_child_t* child = NULL;
    uint32_t index = 0;
    if (controller.descriptor->state->child_refs != UINT32_MAX) {
        for (uint32_t i = 0; i < NX_DEVICE_SPI_MAX_CHILDREN; ++i) {
            if (children[i].phase == SPI_FREE && children[i].generation != UINT64_MAX) {
                child = &children[i]; index = i + 1u; break;
            }
        }
    }
    if (child) {
        spi_child_t next = {0};
        next.generation = child->generation + 1u;
        next.next_ticket = child->next_ticket;
        next.controller = controller;
        next.phase = SPI_OPENING;
        *child = next;
        ++controller.descriptor->state->child_refs;
    }
    leave(saved);
    if (!child) {
        nx_device_dispatch_unpin(controller);
        return NX_ERR_NO_RESOURCE;
    }
    status = bus->open_device(bus, config, &child->driver);
    bool malformed = status == NX_OK &&
        (child->driver.owner != bus || !child->driver.token || !child->driver.transfer);
    nx_status_t cleanup = NX_OK;
    if (malformed) {
        status = NX_ERR_INVALID_STATE;
        cleanup = bus->close_device(bus, &child->driver);
    }
    saved = enter();
    if (status == NX_OK) {
        child->phase = SPI_READY;
        *out = (nx_device_spi_ref_t){controller, child->generation, index};
    } else if (cleanup != NX_OK) {
        child->phase = SPI_QUARANTINED;
        status = cleanup;
    } else {
        child->phase = SPI_FREE;
        --controller.descriptor->state->child_refs;
    }
    leave(saved);
    nx_device_dispatch_unpin(controller);
    return status;
}

nx_status_t nx_device_spi_close(nx_device_spi_ref_t ref) {
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    status = !child ? NX_ERR_INVALID_STATE : busy(child) ? NX_ERR_BUSY : NX_OK;
    if (status == NX_OK) child->phase = SPI_CLOSING;
    leave(saved);
    if (status == NX_OK) {
        status = bus->close_device ? bus->close_device(bus, &child->driver) : NX_ERR_NOT_SUPPORTED;
        saved = enter();
        child->phase = status == NX_OK ? SPI_FREE : SPI_READY;
        if (status == NX_OK) --ref.controller.descriptor->state->child_refs;
        leave(saved);
    }
    nx_device_dispatch_unpin(ref.controller);
    return status;
}

nx_status_t nx_device_spi_recover(nx_device_ref_t controller) {
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(controller, &bus, true);
    if (status != NX_OK) return status;
    bool found = false;
    for (uint32_t i = 0; i < NX_DEVICE_SPI_MAX_CHILDREN; ++i) {
        uint32_t saved = enter();
        spi_child_t* child = &children[i];
        bool selected = child->phase == SPI_QUARANTINED && same_controller(child->controller, controller);
        if (selected) child->phase = SPI_CLOSING;
        leave(saved);
        if (!selected) continue;
        found = true;
        status = bus->close_device ? bus->close_device(bus, &child->driver) : NX_ERR_NOT_SUPPORTED;
        saved = enter();
        child->phase = status == NX_OK ? SPI_FREE : SPI_QUARANTINED;
        if (status == NX_OK) --controller.descriptor->state->child_refs;
        leave(saved);
        if (status != NX_OK) break;
    }
    nx_device_dispatch_unpin(controller);
    return found ? status : NX_ERR_NO_DATA;
}

nx_status_t nx_device_spi_query(nx_device_spi_ref_t ref, nx_device_caps_t* caps) {
    if (!caps) return NX_ERR_NULL_PTR;
    memset(caps, 0, sizeof(*caps));
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    if (!child) status = NX_ERR_INVALID_STATE;
    else {
        caps->device_class = NX_DEVICE_CLASS_SPI;
        caps->flags = NX_DEVICE_CAP_SPI_DEVICES;
        if (child->driver.submit && bus->service) caps->flags |= NX_DEVICE_CAP_SPI_QUEUE;
        if (child->driver.cancel) caps->flags |= NX_DEVICE_CAP_SPI_CANCEL;
    }
    leave(saved);
    nx_device_dispatch_unpin(ref.controller);
    return status;
}

/* Parent pin and child running flag remain held until the provider returns. */
static void synchronous_terminal(void* context, nx_status_t status) {
    spi_child_t* child = context;
    uint32_t saved = enter();
    child->callback_active = true;
    leave(saved);
    if (child->callback) child->callback(child->user_data, status);
    saved = enter();
    child->callback_active = false;
    leave(saved);
}
nx_status_t nx_device_spi_transfer(nx_device_spi_ref_t ref,
                                  const nx_spi_transaction_t* transaction) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    if (!transaction_valid(transaction, false)) return NX_ERR_INVALID_PARAM;
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    status = !child ? NX_ERR_INVALID_STATE : busy(child) ? NX_ERR_BUSY :
             !transaction->timeout_ms ? NX_ERR_TIMEOUT : NX_OK;
    if (status == NX_OK) {
        child->running = true;
        child->callback = transaction->callback;
        child->user_data = transaction->user_data;
    }
    leave(saved);
    if (status == NX_OK) {
        nx_spi_transaction_t wrapped = *transaction;
        wrapped.callback = transaction->callback ? synchronous_terminal : NULL;
        wrapped.user_data = child;
        status = child->driver.transfer(&child->driver, &wrapped);
        saved = enter();
        child->running = false;
        child->callback = NULL;
        child->user_data = NULL;
        leave(saved);
    }
    nx_device_dispatch_unpin(ref.controller);
    return status;
}

static void queued_terminal(void* context, nx_status_t status) {
    spi_child_t* child = context;
    uint32_t saved = enter();
    child->callback_active = true;
    child->terminal_status = status;
    nx_spi_terminal_callback_t callback = child->callback;
    void* user_data = child->user_data;
    leave(saved);
    if (callback) callback(user_data, status);
    saved = enter();
    child->callback_active = false;
    child->settled = true;
    child->active_ticket = 0;
    child->callback = NULL;
    child->user_data = NULL;
    leave(saved);
}
nx_status_t nx_device_spi_submit(nx_device_spi_ref_t ref,
                                const nx_spi_transaction_t* transaction,
                                nx_device_spi_ticket_t* ticket) {
    if (!ticket) return NX_ERR_NULL_PTR;
    memset(ticket, 0, sizeof(*ticket));
    if (!transaction_valid(transaction, true)) return NX_ERR_INVALID_PARAM;
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    status = !child ? NX_ERR_INVALID_STATE : busy(child) ? NX_ERR_BUSY :
             !child->driver.submit || !bus->service ? NX_ERR_NOT_SUPPORTED :
             child->next_ticket == UINT64_MAX ? NX_ERR_NO_RESOURCE : NX_OK;
    uint64_t previous_ticket = 0;
    nx_status_t previous_status = NX_OK;
    bool previous_settled = false;
    if (status == NX_OK) {
        previous_ticket = child->last_ticket;
        previous_status = child->terminal_status;
        previous_settled = child->settled;
        child->active_ticket = ++child->next_ticket;
        child->last_ticket = child->active_ticket;
        child->submitting = true;
        child->settled = false;
        child->terminal_status = NX_ERR_BUSY;
        child->callback = transaction->callback;
        child->user_data = transaction->user_data;
        *ticket = (nx_device_spi_ticket_t){child->active_ticket, ref.generation, ref.slot};
    }
    leave(saved);
    if (status == NX_OK) {
        nx_spi_transaction_t wrapped = *transaction;
        wrapped.callback = queued_terminal;
        wrapped.user_data = child;
        status = child->driver.submit(&child->driver, &wrapped);
        saved = enter();
        child->submitting = false;
        if (status != NX_OK) {
            child->active_ticket = 0;
            child->last_ticket = previous_ticket;
            child->terminal_status = previous_status;
            child->settled = previous_settled;
            child->callback = NULL;
            child->user_data = NULL;
            memset(ticket, 0, sizeof(*ticket));
        }
        leave(saved);
    }
    nx_device_dispatch_unpin(ref.controller);
    return status;
}
static bool ticket_valid(nx_device_spi_ref_t ref, const spi_child_t* child,
                         nx_device_spi_ticket_t ticket) {
    return ticket.sequence && ticket.generation == ref.generation && ticket.slot == ref.slot &&
           ticket.sequence == child->last_ticket;
}
nx_status_t nx_device_spi_poll(nx_device_spi_ref_t ref,
                              nx_device_spi_ticket_t ticket,
                              nx_device_spi_result_t* result) {
    if (!result) return NX_ERR_NULL_PTR;
    *result = (nx_device_spi_result_t){NX_ERR_INVALID_STATE, false};
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    if (!child || !ticket_valid(ref, child, ticket)) status = NX_ERR_INVALID_STATE;
    else *result = (nx_device_spi_result_t){child->settled ? child->terminal_status : NX_ERR_BUSY,
                                           child->settled};
    leave(saved);
    nx_device_dispatch_unpin(ref.controller);
    return status;
}
nx_status_t nx_device_spi_cancel(nx_device_spi_ref_t ref,
                                nx_device_spi_ticket_t ticket) {
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    status = !child || !ticket_valid(ref, child, ticket) ? NX_ERR_INVALID_STATE :
             child->submitting || child->cancelling || child->callback_active ? NX_ERR_BUSY :
             !child->active_ticket ? NX_ERR_NO_DATA :
             !child->driver.cancel ? NX_ERR_NOT_SUPPORTED : NX_OK;
    if (status == NX_OK) child->cancelling = true;
    leave(saved);
    if (status == NX_OK) {
        status = child->driver.cancel(&child->driver);
        saved = enter();
        child->cancelling = false;
        /* Even NX_OK only accepts a request. Callback is the settlement proof. */
        leave(saved);
    }
    nx_device_dispatch_unpin(ref.controller);
    return status;
}
nx_status_t nx_device_spi_service(nx_device_ref_t controller) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (nx_arch_irq_is_masked()) return NX_ERR_INVALID_STATE;
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(controller, &bus, true);
    if (status != NX_OK) return status;
    status = bus->service ? bus->service(bus) : NX_ERR_NOT_SUPPORTED;
    nx_device_dispatch_unpin(controller);
    return status;
}
nx_status_t nx_device_spi_cancel_transfer(nx_device_spi_ref_t ref) {
    nx_spi_bus_t* bus = NULL;
    nx_status_t status = parent_pin(ref.controller, &bus, false);
    if (status != NX_OK) return status;
    uint32_t saved = enter();
    spi_child_t* child = resolve(ref);
    status = !child ? NX_ERR_INVALID_STATE :
             child->cancelling || child->callback_active ? NX_ERR_BUSY :
             !child->running ? NX_ERR_NO_DATA :
             !child->driver.cancel ? NX_ERR_NOT_SUPPORTED : NX_OK;
    if (status == NX_OK) child->cancelling = true;
    leave(saved);
    if (status == NX_OK) {
        status = child->driver.cancel(&child->driver);
        saved = enter();
        child->cancelling = false;
        leave(saved);
    }
    nx_device_dispatch_unpin(ref.controller);
    return status;
}

/**
 * \file            probe.c
 * \brief           Linked callback versus shared-table Wait ABI cost model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/os/wait.h"

_Static_assert(sizeof(void*) == 4, "The cost model requires ARM pointer width");
_Static_assert(sizeof(nx_wait_port_t) == 16,
               "Changing the public Wait ABI requires reviewing this model");

#if SHARED
/** \brief Experimental readonly table, never included in production. */
typedef struct {
    uint32_t (*arm)(void*);
    nx_result_t (*wait)(void*, uint32_t, uint64_t);
    nx_result_t (*wake)(void*);
} probe_ops_t;
/** \brief Experimental face sharing one readonly table. */
typedef struct {
    void* context;
    const probe_ops_t* ops;
} probe_port_t;
#else
typedef nx_wait_port_t probe_port_t;
#endif

static uint32_t states[COUNT];
volatile uint32_t result;
volatile uint32_t index;

/** \brief Both layouts use identical provider bodies. */
static uint32_t arm(void* context) {
    return *(uint32_t*)context;
}

/** \brief Retain actual callback argument passage and return ABI. */
static nx_result_t wait(void* context, uint32_t sequence, uint64_t deadline) {
    return *(uint32_t*)context == sequence && deadline != 0 ? NX_ERROR_BUSY
                                                            : NX_SUCCESS;
}

/** \brief Keep the same mutable context for both measured layouts. */
static nx_result_t wake(void* context) {
    ++*(uint32_t*)context;
    return NX_SUCCESS;
}

#if SHARED
static const probe_ops_t ops = {arm, wait, wake};
#define FACE(i)                                                                \
    { &states[i], &ops }
#else
#define FACE(i)                                                                \
    { &states[i], arm, wait, wake }
#endif

static probe_port_t ports[COUNT] = {
    FACE(0),
#if COUNT > 1
    FACE(1),
#endif
#if COUNT > 2
    FACE(2), FACE(3),
#endif
#if COUNT > 4
    FACE(4), FACE(5), FACE(6), FACE(7),
#endif
};

/** \brief External visibility prevents whole-program constant folding. */
__attribute__((noinline)) uint32_t call_arm(probe_port_t* port) {
#if SHARED
    return port->ops->arm(port->context);
#else
    return port->arm(port->context);
#endif
}

/** \brief Retain the actual 64-bit deadline argument dispatch. */
__attribute__((noinline)) nx_result_t call_wait(probe_port_t* port) {
#if SHARED
    return port->ops->wait(port->context, 1, 500);
#else
    return port->wait(port->context, 1, 500);
#endif
}

/** \brief Retain the highest-frequency notification operation. */
__attribute__((noinline)) nx_result_t call_wake(probe_port_t* port) {
#if SHARED
    return port->ops->wake(port->context);
#else
    return port->wake(port->context);
#endif
}

/** \brief Link-only entry; it supplies no startup, scheduler or board code. */
void _start(void) {
    probe_port_t* port = &ports[index % COUNT];
    result = call_arm(port);
    result += call_wait(port);
    result += call_wake(port);
    for (;;) {
        __asm__ volatile("nop");
    }
}

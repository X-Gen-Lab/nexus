/**
 * \file            storage.h
 * \brief           Assembly-only selected static OS layouts and stack storage
 * \author          Nexus Team
 * \note            Business headers use the opaque public object references.
 *                  Include this header only where actual storage is defined.
 *                  Link exactly one task backend; native kernel headers remain
 *                  an assembly dependency, not a business API. Zero-initialize
 *                  controls before first use. Never change live private fields.
 */
#ifndef NEXUS_OS_STORAGE_H
#define NEXUS_OS_STORAGE_H
#include "nexus/os/channel.h"
#include "nexus/os/notify.h"
#include "nexus/os/queue.h"
#include "nexus/os/task.h"
#if defined(__has_include)
#if !__has_include("nexus/os/detail/backend_storage.h")
#error "Static task storage requires a selected Nexus task backend"
#endif
#endif
#include "nexus/os/detail/backend_storage.h"

#ifdef __cplusplus
#define NX_OS_STACK_ALIGN(value) alignas(value)
#define NX_OS_STACK_ASSERT(condition)                                          \
    static_assert(condition, "Invalid stack capacity")
#else
#define NX_OS_STACK_ALIGN(value) _Alignas(value)
#define NX_OS_STACK_ASSERT(condition)                                          \
    _Static_assert(condition, "Invalid stack capacity")
#endif
/**
 * \brief Declare static native-word storage, rounded up to a positive byte size
 * \note File/block scope only. Pass sizeof(name) as actual capacity to start.
 *       The target compiler owns sizeof/alignment; no maximum union is used.
 */
#define NX_OS_STACK_DEFINE(name, bytes)                                        \
    NX_OS_STACK_ASSERT((bytes) > 0);                                           \
    NX_OS_STACK_ALIGN(NX_OS_STACK_ALIGNMENT)                                   \
    static nx_os_stack_word_t name[((bytes)-1) / sizeof(nx_os_stack_word_t) + 1]
#endif /* NEXUS_OS_STORAGE_H */

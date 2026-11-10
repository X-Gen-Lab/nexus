/**
 * \file            fsmc_nand.c
 * \brief           Finite raw NAND page/OOB and bounded hardware ECC progress
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_fsmc_io.h"
#include "stm32f407_fsmc_nand.h"
#include "stm32f4xx_ll_fsmc.h"
#ifndef NX_STM32_FSMC_READ
#define NX_STM32_FSMC_READ(reg) (reg)
#endif
/** \brief No IRQ/worker: CPU stream, device-ready wait and ECC drain are
 * distinct. */
enum {
    IDLE = 0,
    ARM,
    WAIT_READ,
    READ_DATA,
    WRITE_DATA,
    WAIT_PULSE,
    WAIT_RESET,
    WAIT_INITIAL
};
static bool state_valid(const nx_stm32_fsmc_nand_t* n) {
    return nx_fsmc_span(n, sizeof(*n), _Alignof(nx_stm32_fsmc_nand_t), true);
}
static bool storage(const nx_stm32_fsmc_nand_t* n, const void* p, size_t bytes,
                    bool writable) {
    return nx_fsmc_span(p, bytes, 1U, writable) &&
           !nx_fsmc_overlap(p, bytes, n, sizeof(*n)) &&
           !nx_fsmc_overlap(p, bytes, n->config, sizeof(*n->config)) &&
           !nx_fsmc_overlap(p, bytes, n->config->geometry,
                            sizeof(*n->config->geometry)) &&
           nx_stm32_fsmc_storage(n->lease.owner, p, bytes, writable);
}
/** \brief Official HAL NAND uses byte command/address cycles for both widths.
 */
static void command(nx_stm32_fsmc_nand_t* n, uint8_t value) {
    nx_fsmc_write(n->lease.address | UINT32_C(0x10000), value, 8U);
}
static void address(nx_stm32_fsmc_nand_t* n, uint8_t value) {
    nx_fsmc_write(n->lease.address | UINT32_C(0x20000), value, 8U);
}
static uint8_t raw_status(nx_stm32_fsmc_nand_t* n) {
    command(n, 0x70U);
    return (uint8_t)nx_fsmc_read(n->lease.address, n->width);
}
static volatile uint32_t* pcr(nx_stm32_fsmc_nand_t* n) {
    return n->bank == 2U ? &FSMC_Bank2_3->PCR2 : &FSMC_Bank2_3->PCR3;
}
static volatile uint32_t* sr(nx_stm32_fsmc_nand_t* n) {
    return n->bank == 2U ? &FSMC_Bank2_3->SR2 : &FSMC_Bank2_3->SR3;
}
static volatile uint32_t* eccr(nx_stm32_fsmc_nand_t* n) {
    return n->bank == 2U ? &FSMC_Bank2_3->ECCR2 : &FSMC_Bank2_3->ECCR3;
}
static nx_result_t clock_now(nx_stm32_fsmc_nand_t* n, nx_time_us_t* now) {
    const nx_stm32_fsmc_t* bus = n->lease.owner;
    *now = bus->config->clock.read(bus->config->clock.context);
    if (*now == NX_DEADLINE_NEVER || *now < n->last_clock)
        return NX_ERROR_STATE;
    n->last_clock = *now;
    return NX_SUCCESS;
}
static bool geometry_valid(const nx_nand_geometry_t* g,
                           const nx_stm32_fsmc_parameters_t* p) {
    if (!nx_fsmc_span(g, sizeof(*g), _Alignof(nx_nand_geometry_t), false))
        return false;
    return g->width == p->width && g->ecc_bytes == p->ecc_bytes &&
           g->page_bytes >= 512U && g->page_bytes <= 8192U &&
           (g->page_bytes & (g->page_bytes - 1U)) == 0U && g->oob_bytes != 0U &&
           g->oob_bytes <= 1024U && g->oob_bytes % (g->width / 8U) == 0U &&
           g->pages_per_block != 0U && g->blocks != 0U && g->row_cycles >= 2U &&
           g->row_cycles <= 3U &&
           (uint64_t)g->pages_per_block * g->blocks <=
               (UINT64_C(1) << (8U * g->row_cycles)) &&
           g->column_cycles == (g->page_bytes == 512U ? 1U : 2U) &&
           g->ecc_bytes <= g->page_bytes && g->page_bytes % g->ecc_bytes == 0U;
}
nx_result_t
nx_stm32_fsmc_nand_prepare(nx_stm32_fsmc_nand_t* n, nx_stm32_fsmc_t* bus,
                           size_t index,
                           const nx_stm32_fsmc_nand_config_t* config) {
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) ||
        !nx_fsmc_span(bus, sizeof(*bus), _Alignof(nx_stm32_fsmc_t), true) ||
        !nx_fsmc_span(config, sizeof(*config),
                      _Alignof(nx_stm32_fsmc_nand_config_t), false))
        return NX_ERROR_INVALID;
    if (n->lease.owner != NULL || n->config != NULL || n->request != NULL ||
        n->in_call)
        return NX_ERROR_BUSY;
    if (!bus->prepared || bus->config == NULL ||
        index >= bus->config->bank_count)
        return NX_ERROR_STATE;
    if (bus->config->banks[index].kind != NX_STM32_FSMC_NAND ||
        bus->config->parameters == NULL)
        return NX_ERROR_UNSUPPORTED;
    const nx_stm32_fsmc_parameters_t* p = &bus->config->parameters[index];
    if (!geometry_valid(config->geometry, p) ||
        config->ready_delay_us > 1000000U ||
        !nx_stm32_fsmc_storage(bus, n, sizeof(*n), true) ||
        nx_fsmc_overlap(n, sizeof(*n), config, sizeof(*config)) ||
        nx_fsmc_overlap(n, sizeof(*n), config->geometry,
                        sizeof(*config->geometry)))
        return NX_ERROR_INVALID;
    nx_time_us_t now = bus->config->clock.read(bus->config->clock.context);
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32_fsmc_acquire(bus, index, &n->lease);
    if (result != NX_SUCCESS)
        return result;
    n->config = config;
    n->width = p->width;
    n->bank = p->bank;
    n->last_clock = now;
    n->closed = true;
    n->phase = WAIT_INITIAL;
    if ((raw_status(n) & 0x40U) == 0U)
        return NX_ERROR_BUSY;
    n->phase = IDLE;
    n->closed = false;
    return NX_SUCCESS;
}
static void terminal(nx_stm32_fsmc_nand_t* n, nx_result_t result) {
    if (n->terminal == NX_SUCCESS)
        n->terminal = result;
}
static bool read_operation(nx_nand_operation_t op) {
    return op == NX_NAND_READ_PAGE || op == NX_NAND_READ_OOB;
}
static bool write_operation(nx_nand_operation_t op) {
    return op == NX_NAND_PROGRAM_PAGE || op == NX_NAND_PROGRAM_OOB;
}
static void settle(nx_stm32_fsmc_nand_t* n) {
    nx_nand_request_t* r = n->request;
    size_t done = n->terminal == NX_SUCCESS ? r->bytes + r->oob_bytes : 0U;
    nx_result_t result = n->terminal;
    n->request = NULL;
    n->phase = IDLE;
    nx_arch_dsb();
    nx_request_settle(&r->base, result, done);
}
/** \brief ECC toggles use official LL; no tick-based LL_GetECC is selected. */
static nx_result_t ecc_enable(nx_stm32_fsmc_nand_t* n, bool enable) {
    uint32_t bank = n->bank == 2U ? FSMC_NAND_BANK2 : FSMC_NAND_BANK3;
    /* Record an enable attempt before the first hardware effect. A failed
     * observation never erases responsibility for a possibly active engine. */
    if (enable)
        n->ecc_active = true;
    HAL_StatusTypeDef result = enable
                                   ? FSMC_NAND_ECC_Enable(FSMC_Bank2_3, bank)
                                   : FSMC_NAND_ECC_Disable(FSMC_Bank2_3, bank);
    nx_arch_dsb();
    if (result != HAL_OK ||
        ((NX_STM32_FSMC_READ(*pcr(n)) & FSMC_PCR2_ECCEN) != 0U) != enable)
        return NX_ERROR_IO;
    n->ecc_active = enable;
    return NX_SUCCESS;
}
/** \brief Capture only after actual FIFO drain, then disable/read back ECC. */
static nx_result_t ecc_drain(nx_stm32_fsmc_nand_t* n) {
    if (!n->ecc_active && !n->ecc_pending)
        return NX_SUCCESS;
    if ((NX_STM32_FSMC_READ(*sr(n)) & FSMC_SR2_FEMPT) == 0U)
        return NX_ERROR_BUSY;
    if (n->ecc_pending) {
        n->request->ecc[n->ecc_done] = NX_STM32_FSMC_READ(*eccr(n));
        ++n->ecc_done;
        n->ecc_pending = false;
    }
    return ecc_enable(n, false);
}
/** \brief Include a full Core microsecond quantization margin after a command.
 */
static void wait_phase(nx_stm32_fsmc_nand_t* n, uint8_t phase) {
    nx_time_us_t now;
    nx_result_t result = clock_now(n, &now);
    if (result != NX_SUCCESS && n->request != NULL)
        terminal(n, result);
    uint64_t delay = n->config->ready_delay_us;
    if (delay != 0U)
        ++delay;
    n->poll_after = n->last_clock > NX_DEADLINE_NEVER - 1U - delay
                        ? NX_DEADLINE_NEVER
                        : n->last_clock + delay;
    n->phase = phase;
}
static void row_address(nx_stm32_fsmc_nand_t* n, uint32_t page) {
    for (uint8_t i = 0U; i < n->config->geometry->row_cycles; ++i)
        address(n, (uint8_t)(page >> (8U * i)));
}
static void page_address(nx_stm32_fsmc_nand_t* n) {
    nx_nand_request_t* r = n->request;
    const nx_nand_geometry_t* g = n->config->geometry;
    uint32_t column = r->bytes == 0U && g->page_bytes > 512U
                          ? g->page_bytes / (g->width / 8U)
                          : 0U;
    for (uint8_t i = 0U; i < g->column_cycles; ++i)
        address(n, (uint8_t)(column >> (8U * i)));
    row_address(n, r->page);
}
/** \brief No command is issued until admission and observed device readiness.
 */
static nx_result_t arm(nx_stm32_fsmc_nand_t* n) {
    nx_nand_request_t* r = n->request;
    uint8_t status = raw_status(n);
    if ((status & 0x40U) == 0U)
        return NX_ERROR_BUSY;
    if (n->terminal != NX_SUCCESS) {
        settle(n);
        return NX_SUCCESS;
    }
    if ((write_operation(r->operation) ||
         r->operation == NX_NAND_ERASE_BLOCK) &&
        (status & 0x80U) == 0U) {
        terminal(n, NX_ERROR_PERMISSION);
        settle(n);
        return NX_SUCCESS;
    }
    if (r->operation == NX_NAND_RESET) {
        command(n, 0xffU);
        wait_phase(n, WAIT_RESET);
        return NX_SUCCESS;
    }
    if (r->operation == NX_NAND_ERASE_BLOCK) {
        command(n, 0x60U);
        row_address(n, r->page);
        command(n, 0xd0U);
        wait_phase(n, WAIT_PULSE);
        return NX_SUCCESS;
    }
    if (read_operation(r->operation)) {
        command(n, r->bytes == 0U && n->config->geometry->page_bytes == 512U
                       ? 0x50U
                       : 0x00U);
        page_address(n);
        if (n->config->geometry->page_bytes > 512U)
            command(n, 0x30U);
        wait_phase(n, WAIT_READ);
    } else {
        if (n->config->geometry->page_bytes == 512U)
            command(n, r->bytes == 0U ? 0x50U : 0x00U);
        command(n, 0x80U);
        page_address(n);
        n->phase = WRITE_DATA;
    }
    return NX_SUCCESS;
}
/** \brief At most 32 explicit bus units; ECC boundaries stop this call early.
 */
static nx_result_t stream(nx_stm32_fsmc_nand_t* n) {
    nx_nand_request_t* r = n->request;
    const nx_nand_geometry_t* g = n->config->geometry;
    nx_result_t result =
        n->ecc_pending || n->terminal != NX_SUCCESS ? ecc_drain(n) : NX_SUCCESS;
    if (result != NX_SUCCESS)
        return result;
    if (n->terminal != NX_SUCCESS) {
        if (n->phase == WRITE_DATA) {
            command(n, 0xffU);
            wait_phase(n, WAIT_RESET);
            return NX_ERROR_BUSY;
        }
        settle(n);
        return NX_SUCCESS;
    }
    if (r->ecc != NULL && n->done < r->bytes && !n->ecc_active) {
        result = ecc_enable(n, true);
        if (result != NX_SUCCESS)
            return result;
    }
    size_t total = r->bytes + r->oob_bytes;
    size_t unit = n->width / 8U;
    for (size_t count = 0U; count < 32U && n->done < total; ++count) {
        uint8_t* data = n->done < r->bytes
                            ? (uint8_t*)r->data + n->done
                            : (uint8_t*)r->oob + n->done - r->bytes;
        if (n->phase == READ_DATA) {
            uint16_t value = nx_fsmc_read(n->lease.address, n->width);
            data[0] = (uint8_t)value;
            if (unit == 2U)
                data[1] = (uint8_t)(value >> 8U);
        } else {
            uint16_t value = data[0];
            if (unit == 2U)
                value = (uint16_t)(value | (uint16_t)((uint16_t)data[1] << 8U));
            nx_fsmc_write(n->lease.address, value, n->width);
        }
        n->done += unit;
        if (n->ecc_active && n->done <= r->bytes &&
            n->done % g->ecc_bytes == 0U) {
            n->ecc_pending = true;
            return NX_SUCCESS;
        }
    }
    nx_arch_dsb();
    if (n->done == total) {
        if (n->phase == WRITE_DATA) {
            nx_time_us_t now;
            result = clock_now(n, &now);
            if (result != NX_SUCCESS)
                terminal(n, result);
            else if (now >= r->base.deadline)
                terminal(n, NX_ERROR_TIMEOUT);
            if (n->terminal != NX_SUCCESS) {
                command(n, 0xffU);
                wait_phase(n, WAIT_RESET);
                return NX_ERROR_BUSY;
            }
            command(n, 0x10U);
            wait_phase(n, WAIT_PULSE);
        } else {
            nx_time_us_t now;
            result = clock_now(n, &now);
            if (result != NX_SUCCESS)
                terminal(n, result);
            else if (now >= r->base.deadline)
                terminal(n, NX_ERROR_TIMEOUT);
            settle(n);
        }
    }
    return NX_SUCCESS;
}
static nx_result_t service_inner(nx_stm32_fsmc_nand_t* n) {
    nx_result_t result = nx_stm32_fsmc_check(&n->lease);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t now;
    result = clock_now(n, &now);
    if (n->request != NULL) {
        if (result != NX_SUCCESS)
            terminal(n, result);
        else if (now >= n->request->base.deadline)
            terminal(n, NX_ERROR_TIMEOUT);
    } else if (result != NX_SUCCESS)
        return result;
    if (n->phase == WAIT_INITIAL) {
        if ((raw_status(n) & 0x40U) == 0U)
            return NX_ERROR_BUSY;
        n->phase = IDLE;
        return NX_SUCCESS;
    }
    if (n->request == NULL)
        return NX_SUCCESS;
    if (n->phase == ARM)
        return arm(n);
    if (n->phase == WAIT_READ || n->phase == WAIT_PULSE ||
        n->phase == WAIT_RESET) {
        if (n->config->ready_delay_us != 0U &&
            (result != NX_SUCCESS || now < n->poll_after))
            return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
        uint8_t status = raw_status(n);
        if ((status & 0x40U) == 0U)
            return NX_ERROR_BUSY;
        if ((status & 1U) != 0U)
            terminal(n, NX_ERROR_IO);
        result = clock_now(n, &now);
        if (result != NX_SUCCESS)
            terminal(n, result);
        else if (now >= n->request->base.deadline)
            terminal(n, NX_ERROR_TIMEOUT);
        if (n->phase != WAIT_READ || n->terminal != NX_SUCCESS) {
            settle(n);
            return NX_SUCCESS;
        }
        command(n, 0x00U);
        n->phase = READ_DATA;
        return NX_SUCCESS;
    }
    return stream(n);
}
static nx_result_t service(void* context) {
    nx_stm32_fsmc_nand_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->config == NULL || n->lease.owner == NULL)
        return NX_ERROR_STATE;
    if (n->in_call)
        return NX_ERROR_BUSY;
    n->in_call = true;
    nx_result_t result = service_inner(n);
    if (n->request != NULL &&
        (result == NX_ERROR_IO || result == NX_ERROR_STATE ||
         (result == NX_ERROR_BUSY && n->terminal != NX_SUCCESS))) {
        if (result != NX_ERROR_BUSY)
            terminal(n, result);
        (void)nx_request_transition(&n->request->base, NX_REQUEST_QUARANTINED);
    }
    n->in_call = false;
    return result;
}
/** \brief Validate every exact page/OOB/ECC span before admitting any command.
 */
static bool request_valid(nx_stm32_fsmc_nand_t* n, nx_nand_request_t* r) {
    const nx_nand_geometry_t* g = n->config->geometry;
    if (!storage(n, r, sizeof(*r), true) ||
        r->base.deadline == NX_DEADLINE_NEVER ||
        (unsigned)r->operation > (unsigned)NX_NAND_RESET ||
        (uint64_t)r->page >= (uint64_t)g->pages_per_block * g->blocks)
        return false;
    bool page = r->operation == NX_NAND_READ_PAGE ||
                r->operation == NX_NAND_PROGRAM_PAGE;
    bool oob =
        r->operation == NX_NAND_READ_OOB || r->operation == NX_NAND_PROGRAM_OOB;
    if (page ? r->bytes != g->page_bytes : r->bytes != 0U)
        return false;
    if (r->bytes == 0U && r->data != NULL)
        return false;
    if (oob ? r->oob_bytes != g->oob_bytes
            : (r->oob_bytes != 0U && (!page || r->oob_bytes != g->oob_bytes)))
        return false;
    if (r->oob_bytes == 0U && r->oob != NULL)
        return false;
    if (r->operation == NX_NAND_ERASE_BLOCK &&
        r->page % g->pages_per_block != 0U)
        return false;
    if (r->operation == NX_NAND_RESET && r->page != 0U)
        return false;
    bool read = read_operation(r->operation);
    if (r->bytes != 0U && (!storage(n, r->data, r->bytes, read) ||
                           nx_fsmc_overlap(r, sizeof(*r), r->data, r->bytes)))
        return false;
    if (r->oob_bytes != 0U &&
        (!storage(n, r->oob, r->oob_bytes, read) ||
         nx_fsmc_overlap(r, sizeof(*r), r->oob, r->oob_bytes) ||
         (r->bytes != 0U &&
          nx_fsmc_overlap(r->data, r->bytes, r->oob, r->oob_bytes))))
        return false;
    if (r->ecc == NULL)
        return r->ecc_count == 0U;
    size_t bytes = (g->page_bytes / g->ecc_bytes) * sizeof(uint32_t);
    return page && r->ecc_count == g->page_bytes / g->ecc_bytes &&
           (uintptr_t)r->ecc % _Alignof(uint32_t) == 0U &&
           storage(n, r->ecc, bytes, true) &&
           !nx_fsmc_overlap(r, sizeof(*r), r->ecc, bytes) &&
           !nx_fsmc_overlap(r->data, r->bytes, r->ecc, bytes) &&
           (r->oob_bytes == 0U ||
            !nx_fsmc_overlap(r->oob, r->oob_bytes, r->ecc, bytes));
}
static nx_result_t submit(void* context, nx_nand_request_t* r) {
    nx_stm32_fsmc_nand_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) ||
        !nx_fsmc_span(r, sizeof(*r), _Alignof(nx_nand_request_t), true))
        return NX_ERROR_INVALID;
    if (n->config == NULL || n->closed)
        return NX_ERROR_STATE;
    if (n->request != NULL || n->phase != IDLE || n->in_call)
        return NX_ERROR_BUSY;
    if (!request_valid(n, r))
        return NX_ERROR_INVALID;
    if (nx_request_state(&r->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    nx_result_t result = nx_stm32_fsmc_check(&n->lease);
    if (result != NX_SUCCESS)
        return result;
    nx_time_us_t now;
    result = clock_now(n, &now);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&r->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    n->request = r;
    n->done = 0U;
    n->ecc_done = 0U;
    n->terminal = NX_SUCCESS;
    n->phase = ARM;
    (void)service(n);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_nand_request_t* r) {
    nx_stm32_fsmc_nand_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) || r == NULL)
        return NX_ERROR_INVALID;
    if (n->in_call)
        return NX_ERROR_BUSY;
    if (n->request != r)
        return NX_ERROR_STATE;
    terminal(n, NX_ERROR_CANCELLED);
    if (nx_request_state(&r->base) != NX_REQUEST_QUARANTINED)
        (void)nx_request_transition(&r->base, NX_REQUEST_DRAINING);
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_stm32_fsmc_nand_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->in_call)
        return NX_ERROR_BUSY;
    n->closed = true;
    if (n->request != NULL)
        terminal(n, NX_ERROR_CANCELLED);
    return n->config == NULL ? NX_SUCCESS : service(n);
}
nx_result_t nx_stm32_fsmc_nand_close(nx_stm32_fsmc_nand_t* n) {
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    if (n->in_call || n->request != NULL || n->ecc_active || n->ecc_pending)
        return NX_ERROR_BUSY;
    n->closed = true;
    if (n->lease.owner == NULL)
        return NX_SUCCESS;
    if (n->phase != IDLE) {
        nx_result_t result = service(n);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_result_t result = nx_stm32_fsmc_check(&n->lease);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_fsmc_release(&n->lease, n->lease.epoch);
    if (result == NX_SUCCESS)
        n->config = NULL;
    return result;
}
static const nx_nand_geometry_t* geometry(const void* context) {
    const nx_stm32_fsmc_nand_t* n = context;
    return state_valid(n) && n->config != NULL && !n->closed
               ? n->config->geometry
               : NULL;
}
static nx_result_t idle_check(nx_stm32_fsmc_nand_t* n, void* data,
                              size_t bytes) {
    if (n->config == NULL || n->closed)
        return NX_ERROR_STATE;
    if (n->request != NULL || n->phase != IDLE || n->in_call)
        return NX_ERROR_BUSY;
    if (!storage(n, data, bytes, true))
        return NX_ERROR_INVALID;
    return nx_stm32_fsmc_check(&n->lease);
}
static nx_result_t id(void* context, uint8_t* data, size_t bytes) {
    nx_stm32_fsmc_nand_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n) || bytes == 0U || bytes > 8U)
        return NX_ERROR_INVALID;
    nx_result_t result = idle_check(n, data, bytes);
    if (result != NX_SUCCESS)
        return result;
    n->in_call = true;
    command(n, 0x90U);
    address(n, 0U);
    for (size_t i = 0U; i < bytes; ++i)
        data[i] = (uint8_t)nx_fsmc_read(n->lease.address, n->width);
    nx_arch_dsb();
    n->in_call = false;
    return NX_SUCCESS;
}
static nx_result_t status(void* context, uint8_t* value) {
    nx_stm32_fsmc_nand_t* n = context;
    if (!nx_fsmc_task())
        return NX_ERROR_CONTEXT;
    if (!state_valid(n))
        return NX_ERROR_INVALID;
    nx_result_t result = idle_check(n, value, sizeof(*value));
    if (result != NX_SUCCESS)
        return result;
    n->in_call = true;
    *value = raw_status(n);
    n->in_call = false;
    return NX_SUCCESS;
}
const nx_nand_ops_t nx_stm32_fsmc_nand_ops = {geometry, id,      status, submit,
                                              cancel,   service, stop};

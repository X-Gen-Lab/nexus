/**
 * \file            flash.c
 * \brief           GD32F470 full 1 MiB physical Flash and 4 KiB page pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "nexus/arch/arch.h"
#include "private/system.h"
#include <string.h>

#ifndef NX_GD32_FLASH_READ
#define NX_GD32_FLASH_READ(reg) (*(reg))
#endif
#ifndef NX_GD32_FLASH_WRITE
#define NX_GD32_FLASH_WRITE(reg, value) (*(reg) = (value))
#endif
#ifndef NX_GD32_FLASH_IRQ_IDLE
#define NX_GD32_FLASH_IRQ_IDLE()                                               \
    ((NVIC->ISER[(uint32_t)FMC_IRQn >> 5U] &                                   \
      (UINT32_C(1) << ((uint32_t)FMC_IRQn & 31U))) == 0U &&                    \
     NVIC_GetPendingIRQ(FMC_IRQn) == 0U)
#endif
#ifndef NX_GD32_FLASH_IRQ_ACTIVE
#define NX_GD32_FLASH_IRQ_ACTIVE() (NVIC_GetActive(FMC_IRQn) != 0U)
#endif
#ifndef NX_GD32_FLASH_IRQ_CLEAR
/** \brief Join the disabled, exclusively acquired FMC vector. */
static bool clear_irq(void) {
    NVIC_ClearPendingIRQ(FMC_IRQn);
    nx_gd32_peripheral_barrier();
    return NX_GD32_FLASH_IRQ_IDLE() && !NX_GD32_FLASH_IRQ_ACTIVE();
}
#define NX_GD32_FLASH_IRQ_CLEAR() clear_irq()
#endif
#define NX_FMC_ERRORS                                                          \
    (FMC_FLAG_OPERR | FMC_FLAG_WPERR | FMC_FLAG_PGMERR | FMC_FLAG_PGSERR |     \
     FMC_FLAG_RDDERR)
#define NX_FMC_OWNED_MODES                                                     \
    ((uint32_t)(FMC_CTL_PG | FMC_CTL_SER | FMC_CTL_SN | FMC_CTL_START |        \
                FMC_CTL_ENDIE))

static nx_gd32_flash_state_t* s_flash;

/** \brief           Validate physical offsets without addition overflow. */
static bool valid_range(uint32_t offset, size_t length) {
    return offset <= g_nx_gd32f470_flash_geometry.size &&
           length <= g_nx_gd32f470_flash_geometry.size - offset;
}

/** \brief           Wait for the current unpreemptible pulse to really finish.
 */
static void drain(void) {
    while ((FMC_STAT & FMC_STAT_BUSY) != 0u) {
        /* Deadline cannot revoke the physical erase/program pulse. */
    }
    nx_gd32_peripheral_barrier();
}

/** \brief Each pulse needs freshly cleared flags, not a preceding page's END.
 */
static bool clear_status(void) {
    fmc_flag_clear(FMC_FLAG_END | NX_FMC_ERRORS);
    nx_gd32_peripheral_barrier();
    return (NX_GD32_FLASH_READ(&FMC_STAT) &
            (FMC_FLAG_END | NX_FMC_ERRORS | FMC_STAT_BUSY)) == 0U;
}
static bool cold(void) {
    return (NX_GD32_FLASH_READ(&FMC_CTL) &
            ~(uint32_t)(FMC_CTL_LK | FMC_CTL_PSZ)) == 0U &&
           (NX_GD32_FLASH_READ(&FMC_PECFG) & FMC_PE_EN) == 0U &&
           NX_GD32_FLASH_IRQ_IDLE() && !NX_GD32_FLASH_IRQ_ACTIVE();
}
/** \brief Restore sources/lock before allowing Platform to return ownership. */
static nx_result_t end(nx_gd32_flash_state_t* port, nx_result_t status) {
    NX_GD32_FLASH_WRITE(&FMC_CTL,
                        (NX_GD32_FLASH_READ(&FMC_CTL) & ~NX_FMC_OWNED_MODES));
    NX_GD32_FLASH_WRITE(&FMC_PECFG,
                        NX_GD32_FLASH_READ(&FMC_PECFG) & ~FMC_PE_EN);
    fmc_lock();
    nx_gd32_peripheral_barrier();
    bool withdrawn = (NX_GD32_FLASH_READ(&FMC_CTL) &
                      (NX_FMC_OWNED_MODES | FMC_CTL_LK)) == FMC_CTL_LK &&
                     (NX_GD32_FLASH_READ(&FMC_PECFG) & FMC_PE_EN) == 0U;
    withdrawn = clear_status() && withdrawn;
    withdrawn = NX_GD32_FLASH_IRQ_CLEAR() && withdrawn;
    withdrawn = !NX_GD32_FLASH_IRQ_ACTIVE() && withdrawn;
    /* Calls are synchronous and have drained BSY. s_flash/initialized retain
     * the concrete lease on cleanup failure; ordinary stop retries it. */
    port->active = false;
    return withdrawn ? status : NX_ERROR_IO;
}
/** \brief Enable END latching while the NVIC line remains disabled. */
static bool prepare(void) {
    if (!clear_status() || !NX_GD32_FLASH_IRQ_CLEAR())
        return false;
    NX_GD32_FLASH_WRITE(&FMC_CTL, NX_GD32_FLASH_READ(&FMC_CTL) | FMC_CTL_ENDIE);
    nx_gd32_peripheral_barrier();
    return (NX_GD32_FLASH_READ(&FMC_CTL) & FMC_CTL_ENDIE) != 0U;
}
static bool completed(void) {
    uint32_t status = NX_GD32_FLASH_READ(&FMC_STAT);
    return (status & NX_FMC_ERRORS) == 0U && (status & FMC_STAT_END) != 0U;
}

/** \brief           Validate context, then unlock under the single-owner rule.
 */
static nx_result_t begin(nx_gd32_flash_state_t* port) {
    if (!port || port != s_flash || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    uint32_t saved = nx_gd32_critical_enter();
    nx_gd32_critical_leave(saved);
    if (!nx_arch_is_privileged() || nx_gd32_in_isr() || nx_gd32_irq_masked() ||
        saved) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || (FMC_STAT & FMC_STAT_BUSY) != 0u || !cold()) {
        return NX_ERROR_BUSY;
    }
    port->active = true;
    fmc_unlock();
    if ((FMC_CTL & FMC_CTL_LK) != 0u) {
        port->active = false;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

/** \brief           Expose physical geometry only after observed density
 * matches. */
nx_result_t nx_gd32_flash_initialize(nx_gd32_flash_state_t* port) {
    if (!port) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_flash) {
        return NX_ERROR_BUSY;
    }
    if ((FMC_STAT & FMC_STAT_BUSY) != 0U || !cold())
        return NX_ERROR_BUSY;
    if ((*(volatile const uint32_t*)0x1FFF7A20u >> 16) != 1024u) {
        return NX_ERROR_IO;
    }
    fmc_lock();
    if ((FMC_CTL & FMC_CTL_LK) == 0u) {
        return NX_ERROR_IO;
    }
    *port = (nx_gd32_flash_state_t){.initialized = true};
    s_flash = port;
    return NX_SUCCESS;
}

/** \brief           Return immutable full-density page geometry. */
const nx_flash_geometry_t* nx_gd32_flash_geometry(const void* context) {
    const nx_gd32_flash_state_t* port = context;
    return port && port == s_flash && port->initialized
               ? &g_nx_gd32f470_flash_geometry
               : NULL;
}

/** \brief           Copy only validated physical bytes while no pulse is live.
 */
nx_result_t nx_gd32_flash_read(const void* context, uint32_t offset, void* data,
                               size_t length) {
    const nx_gd32_flash_state_t* port = context;
    if (!port || port != s_flash || !port->initialized ||
        !valid_range(offset, length) || (!data && length)) {
        return NX_ERROR_INVALID;
    }
    if (port->active || (FMC_STAT & FMC_STAT_BUSY) != 0u) {
        return NX_ERROR_BUSY;
    }
    if (length) {
        memcpy(
            data,
            (const void*)(uintptr_t)(g_nx_gd32f470_flash_geometry.base_address +
                                     offset),
            length);
    }
    return NX_SUCCESS;
}

/** \brief           Issue aligned halfword pulses and verify each result. */
nx_result_t nx_gd32_flash_program(void* context, uint32_t offset,
                                  const void* data, size_t length,
                                  nx_time_us_t deadline) {
    nx_gd32_flash_state_t* port = context;
    if (!data || !length || !valid_range(offset, length) || offset % 2u ||
        length % 2u) {
        return NX_ERROR_INVALID;
    }
    nx_result_t status = begin(port);
    if (status != NX_SUCCESS) {
        return status;
    }
    const uint8_t* bytes = data;
    for (size_t i = 0u; i < length; i += 2u) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
        uint16_t value;
        memcpy(&value, bytes + i, sizeof(value));
        if (value == UINT16_MAX) {
            continue;
        }
        uint32_t address =
            g_nx_gd32f470_flash_geometry.base_address + offset + (uint32_t)i;
        if ((*(volatile const uint16_t*)(uintptr_t)address & value) != value) {
            status = NX_ERROR_STATE;
            break;
        }
        if (!prepare()) {
            status = NX_ERROR_IO;
            break;
        }
        fmc_state_enum hardware = fmc_halfword_program(address, value);
        drain();
        if (hardware != FMC_READY || !completed() ||
            *(volatile const uint16_t*)(uintptr_t)address != value) {
            status = hardware == FMC_TOERR ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
            break;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
    }
    status = end(port, status);
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Use F470 independent 4 KiB page erase, never fake sectors.
 */
nx_result_t nx_gd32_flash_erase(void* context, uint32_t offset, size_t length,
                                nx_time_us_t deadline) {
    nx_gd32_flash_state_t* port = context;
    if (!length || !valid_range(offset, length) || offset % 4096u ||
        length % 4096u) {
        return NX_ERROR_INVALID;
    }
    nx_result_t status = begin(port);
    if (status != NX_SUCCESS) {
        return status;
    }
    for (size_t i = 0u; i < length; i += 4096u) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            status = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t address =
            g_nx_gd32f470_flash_geometry.base_address + offset + (uint32_t)i;
        /* UM Rev3.3 2.3.4/2.3.5 condition END on ENDIE. Keep its NVIC
         * vector disabled and prove a new END for every page, even if blank. */
        if (!prepare()) {
            status = NX_ERROR_IO;
            break;
        }
        fmc_state_enum hardware = fmc_page_erase(address);
        drain();
        if (hardware != FMC_READY || !completed()) {
            status = hardware == FMC_TOERR ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
            break;
        }
        for (uint32_t j = 0u; j < 4096u; j += 4u) {
            if (*(volatile const uint32_t*)(uintptr_t)(address + j) !=
                UINT32_MAX) {
                status = NX_ERROR_IO;
                break;
            }
        }
        if (status != NX_SUCCESS ||
            nx_deadline_expired(deadline, nx_time_now_us())) {
            if (status == NX_SUCCESS) {
                status = NX_ERROR_TIMEOUT;
            }
            break;
        }
    }
    status = end(port, status);
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Retain ownership if a pulse or lock effect is unsettled. */
nx_result_t nx_gd32_flash_stop(nx_gd32_flash_state_t* port) {
    if (!port || port != s_flash || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || (FMC_STAT & FMC_STAT_BUSY) != 0u) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = end(port, NX_SUCCESS);
    if (result != NX_SUCCESS)
        return result;
    port->initialized = false;
    s_flash = NULL;
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_flash_ops_t nx_gd32_flash_ops = {
    .geometry = nx_gd32_flash_geometry,
    .read = nx_gd32_flash_read,
    .program = nx_gd32_flash_program,
    .erase = nx_gd32_flash_erase,
};

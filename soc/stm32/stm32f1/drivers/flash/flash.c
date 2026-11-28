/**
 * \file            flash.c
 * \brief           STM32F1 FPEC halfword program and page erase
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/flash/instance.h"
#include "nexus/arch/arch.h"
#include "stm32f1xx_hal_flash_ex.h"

#if !defined(FLASH_PAGE_SIZE) || !defined(FLASH_BANK1_END)
#error "The selected Flash controller requires an official FPEC geometry"
#endif
_Static_assert(FLASH_PAGE_SIZE == 1024U || FLASH_PAGE_SIZE == 2048U,
               "Ordinary FPEC requires 1 KiB or 2 KiB pages");

#if defined(FLASH_BANK2_END)
#define NX_F1_FLASH_BANKS      2U
#define NX_F1_FLASH_BANK_BYTES (512U * 1024U)
#define NX_F1_FLASH_REG(bank2, name)                                           \
    ((bank2) ? &NX_F1_FLASH_REGISTERS->name##2 : &NX_F1_FLASH_REGISTERS->name)
_Static_assert(FLASH_BANK1_END + 1U - FLASH_BASE == NX_F1_FLASH_BANK_BYTES &&
                   FLASH_BANK2_END - FLASH_BANK1_END == NX_F1_FLASH_BANK_BYTES,
               "F1 XL bank boundaries must match CMSIS");
_Static_assert(FLASH_SR_BSY == FLASH_SR2_BSY && FLASH_SR_EOP == FLASH_SR2_EOP &&
                   FLASH_SR_PGERR == FLASH_SR2_PGERR &&
                   FLASH_SR_WRPRTERR == FLASH_SR2_WRPRTERR &&
                   FLASH_CR_PG == FLASH_CR2_PG &&
                   FLASH_CR_PER == FLASH_CR2_PER &&
                   FLASH_CR_STRT == FLASH_CR2_STRT &&
                   FLASH_CR_LOCK == FLASH_CR2_LOCK,
               "Both F1 XL banks must share the ordinary FPEC bit positions");
#else
#define NX_F1_FLASH_BANKS 1U
#define NX_F1_FLASH_REG(bank2, name)                                           \
    ((void)(bank2), &NX_F1_FLASH_REGISTERS->name)
#endif

#ifndef NX_F1_FLASH_REGISTERS
#define NX_F1_FLASH_REGISTERS             FLASH
#define NX_F1_FLASH_MEMORY                ((volatile uint8_t*)(uintptr_t)FLASH_BASE)
#define NX_F1_FLASH_READ(reg)             (*(reg))
#define NX_F1_FLASH_WRITE(reg, value)     (*(reg) = (value))
#define NX_F1_FLASH_STORE(address, value) (*(address) = (value))
#define NX_F1_FLASH_IRQ_IDLE()                                                 \
    (NVIC_GetEnableIRQ(FLASH_IRQn) == 0U &&                                    \
     NVIC_GetActive(FLASH_IRQn) == 0U && NVIC_GetPendingIRQ(FLASH_IRQn) == 0U)
#define NX_F1_FLASH_CLOCK()    (RCC->CR)
#define NX_F1_FLASH_SRAM_START ((uintptr_t)SRAM_BASE)
#endif

#define NX_F1_FLASH_FLAGS         (FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP)
#define NX_F1_FLASH_OWNED_CONTROL (FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_LOCK)

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

static bool busy(bool bank2) {
    return (NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, SR)) & FLASH_SR_BSY) != 0U;
}

static bool bank_locked_and_idle(bool bank2) {
    return !busy(bank2) &&
           NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, CR)) == FLASH_CR_LOCK;
}

static bool locked_and_quiet(void) {
    return NX_F1_FLASH_IRQ_IDLE() && bank_locked_and_idle(false) &&
           (NX_F1_FLASH_BANKS == 1U || bank_locked_and_idle(true));
}

static bool bank2_at(uint32_t offset) {
#if defined(FLASH_BANK2_END)
    return offset >= NX_F1_FLASH_BANK_BYTES;
#else
    (void)offset;
    return false;
#endif
}

static bool reviewed_capacity(uint32_t bytes) {
#if defined(FLASH_BANK2_END)
    return bytes == FLASH_BANK2_END + 1U - FLASH_BASE;
#elif defined(STM32F103xB)
    /* The reviewed C8 and RB share this CMSIS device header. */
    return bytes == 64U * 1024U || bytes == FLASH_BANK1_END + 1U - FLASH_BASE;
#else
    return bytes == FLASH_BANK1_END + 1U - FLASH_BASE;
#endif
}

static bool reviewed_spans(const nx_flash_geometry_t* geometry) {
    if (geometry->erase_span_count != NX_F1_FLASH_BANKS ||
        geometry->erase_spans[0].erase_bytes != FLASH_PAGE_SIZE) {
        return false;
    }
#if defined(FLASH_BANK2_END)
    return geometry->erase_spans[1].offset == NX_F1_FLASH_BANK_BYTES &&
           geometry->erase_spans[1].erase_bytes == FLASH_PAGE_SIZE;
#else
    return true;
#endif
}

nx_result_t nx_stm32f1_flash_prepare(nx_stm32f1_flash_state_t* port,
                                     const nx_flash_geometry_t* geometry,
                                     uint32_t supply_mv) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    uint32_t pages;
    if (port == NULL ||
        nx_flash_geometry_sector_count(geometry, &pages) != NX_SUCCESS ||
        geometry->base_address != FLASH_BASE ||
        !reviewed_capacity(geometry->size) || geometry->program_unit != 2U ||
        geometry->program_flags != 0U || !reviewed_spans(geometry)) {
        return NX_ERROR_INVALID;
    }
    /* The reviewed devices share the inclusive 2.0–3.6 V program range. */
    if (supply_mv < 2000U || supply_mv > 3600U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->geometry != NULL || port->phase != NX_STM32F1_FLASH_IDLE ||
        !locked_and_quiet()) {
        return NX_ERROR_BUSY;
    }
    port->geometry = geometry;
    return NX_SUCCESS;
}

static bool control(bool bank2, uint32_t value) {
    NX_F1_FLASH_WRITE(NX_F1_FLASH_REG(bank2, CR), value);
    nx_arch_dsb();
    return NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, CR)) == value;
}

/** \brief Prove a new completion epoch and keep IRQ sources disabled. */
static bool clear_status(bool bank2) {
    NX_F1_FLASH_WRITE(NX_F1_FLASH_REG(bank2, SR), NX_F1_FLASH_FLAGS);
    nx_arch_dsb();
    return (NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, SR)) &
            (NX_F1_FLASH_FLAGS | FLASH_SR_BSY)) == 0U;
}

/** \brief Withdraw only this polling owner's bits after actual idle. */
static nx_result_t withdraw(nx_stm32f1_flash_state_t* port) {
    const bool bank2 = NX_F1_FLASH_BANKS == 2U && port->bank2;
    if (busy(bank2) || !NX_F1_FLASH_IRQ_IDLE() ||
        (NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, CR)) &
         ~(uint32_t)NX_F1_FLASH_OWNED_CONTROL) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!control(bank2, FLASH_CR_LOCK) || !clear_status(bank2) ||
        !locked_and_quiet()) {
        return NX_ERROR_IO;
    }
    nx_arch_isb();
    port->phase = NX_STM32F1_FLASH_IDLE;
    return NX_SUCCESS;
}

static nx_result_t recover(void* context) {
    nx_stm32f1_flash_state_t* port = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port->geometry == NULL) {
        return NX_ERROR_STATE;
    }
    if (port->phase == NX_STM32F1_FLASH_RETAINED) {
        return withdraw(port);
    }
    return port->phase == NX_STM32F1_FLASH_IDLE && locked_and_quiet()
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}

nx_result_t nx_stm32f1_flash_close(nx_stm32f1_flash_state_t* port) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->geometry == NULL) {
        return NX_SUCCESS;
    }
    nx_result_t result = recover(port);
    if (result == NX_SUCCESS) {
        port->geometry = NULL;
    }
    return result;
}

static const nx_flash_geometry_t* geometry(const void* context) {
    const nx_stm32f1_flash_state_t* port = context;
    return port != NULL ? port->geometry : NULL;
}

static bool range(const nx_stm32f1_flash_state_t* port, uint32_t offset,
                  size_t length) {
    const nx_flash_geometry_t* layout = geometry(port);
    return layout != NULL && offset <= layout->size &&
           length <= layout->size - offset;
}

static bool overlaps(const void* data, size_t length, const volatile void* ref,
                     size_t ref_length) {
    uintptr_t start = (uintptr_t)data;
    uintptr_t other = (uintptr_t)ref;
    return length != 0U && start < other + ref_length && other < start + length;
}

static bool buffer(const nx_stm32f1_flash_state_t* port, const void* data,
                   size_t length) {
    return length == 0U ||
           (data != NULL && length <= UINTPTR_MAX - (uintptr_t)data &&
            !overlaps(data, length, port, sizeof(*port)) &&
            !overlaps(data, length, port->geometry, sizeof(*port->geometry)) &&
            !overlaps(data, length, port->geometry->erase_spans,
                      (size_t)NX_F1_FLASH_BANKS *
                          sizeof(*port->geometry->erase_spans)) &&
            !overlaps(data, length, NX_F1_FLASH_REGISTERS,
                      sizeof(*NX_F1_FLASH_REGISTERS)));
}

#if defined(STM32F103xB)
static nx_result_t
maintenance_snapshot(void* context, nx_flash_maintenance_snapshot_t* snapshot) {
    const nx_stm32f1_flash_state_t* port = context;
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port == NULL || port->geometry == NULL) {
        return NX_ERROR_STATE;
    }
    const uintptr_t address = (uintptr_t)snapshot;
    const uintptr_t ram_start = NX_F1_FLASH_SRAM_START;
    /* DS5319: both reviewed medium-density parts have 20 KiB of SRAM. */
    if (address < ram_start ||
        address - ram_start > 20U * 1024U - sizeof(*snapshot) ||
        address % _Alignof(nx_flash_maintenance_snapshot_t) != 0U ||
        !buffer(port, snapshot, sizeof(*snapshot))) {
        return NX_ERROR_INVALID;
    }
    if (port->phase != NX_STM32F1_FLASH_IDLE || !locked_and_quiet()) {
        return NX_ERROR_BUSY;
    }

    const uint32_t options = NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->OBR);
    if ((options & FLASH_OBR_OPTERR) != 0U) {
        return NX_ERROR_IO;
    }

    /* PM0075 §2.4 and Table 9: one active-low WRP bit per four 1-KiB pages.
     * Read protection also protects pages 0–3. Publish complete observations
     * only when the API's per-page bitmap can represent them.
     */
    const uint32_t pages = port->geometry->size / FLASH_PAGE_SIZE;
    const uint32_t groups = UINT32_MAX >> (32U - pages / 4U);
    const bool protected = (options & FLASH_OBR_RDPRT) != 0U;
    uint32_t write_groups =
        ~NX_F1_FLASH_READ(&NX_F1_FLASH_REGISTERS->WRPR) & groups;
    if (protected) {
        write_groups |= 1U;
    }
    if (write_groups > UINT8_MAX) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t pages_protected = 0U;
    for (uint32_t group = 0U; write_groups != 0U; ++group) {
        if ((write_groups & 1U) != 0U) {
            pages_protected |= UINT32_C(15) << (4U * group);
        }
        write_groups >>= 1U;
    }
    *snapshot = (nx_flash_maintenance_snapshot_t){
        .options =
            {
                .read_protection =
                    protected ? NX_FLASH_READ_PROTECTED : NX_FLASH_READ_OPEN,
                .write_protected_sectors = pages_protected,
                .brownout = NX_FLASH_BROWNOUT_UNKNOWN,
                .hardware_watchdog = (options & FLASH_OBR_IWDG_SW) == 0U,
                .reset_on_stop = (options & FLASH_OBR_nRST_STOP) == 0U,
                .reset_on_standby = (options & FLASH_OBR_nRST_STDBY) == 0U,
            },
        .sector_count = pages,
    };
    return NX_SUCCESS;
}

const nx_flash_maintenance_ops_t nx_stm32f1_flash_maintenance_ops = {
    .snapshot = maintenance_snapshot,
};
#endif

static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_stm32f1_flash_state_t* port = context;
    if (!range(port, offset, length) || !buffer(port, data, length) ||
        overlaps(data, length, NX_F1_FLASH_MEMORY, port->geometry->size)) {
        return NX_ERROR_INVALID;
    }
    if (port->phase == NX_STM32F1_FLASH_RETAINED) {
        return NX_ERROR_STATE;
    }
    if (port->phase != NX_STM32F1_FLASH_IDLE || busy(false) ||
        (NX_F1_FLASH_BANKS == 2U && busy(true))) {
        return NX_ERROR_BUSY;
    }
    uint8_t* output = data;
    for (size_t index = 0U; index < length; ++index) {
        output[index] = NX_F1_FLASH_MEMORY[offset + index];
    }
    return NX_SUCCESS;
}

static bool expired(nx_time_us_t deadline) {
    return nx_deadline_expired(deadline, nx_time_now_us());
}

/** \brief An accepted pulse is non-preemptible, including after its deadline.
 */
static nx_result_t pulse(bool bank2, nx_time_us_t deadline) {
    uint32_t status;
    do {
        status = NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, SR));
    } while ((status & FLASH_SR_BSY) != 0U);
    if ((status & NX_F1_FLASH_FLAGS) != FLASH_SR_EOP) {
        return NX_ERROR_IO;
    }
    return expired(deadline) ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

static nx_result_t finish(nx_stm32f1_flash_state_t* port, nx_result_t result,
                          nx_time_us_t deadline) {
    port->phase = NX_STM32F1_FLASH_RETAINED;
    if (withdraw(port) != NX_SUCCESS) {
        return NX_ERROR_IO;
    }
    return result == NX_SUCCESS && expired(deadline) ? NX_ERROR_TIMEOUT
                                                     : result;
}

static bool clock_ready(void) {
    const uint32_t needed = RCC_CR_HSION | RCC_CR_HSIRDY;
    return (NX_F1_FLASH_CLOCK() & needed) == needed;
}

static nx_result_t begin(nx_stm32f1_flash_state_t* port, bool bank2,
                         nx_time_us_t deadline) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->phase == NX_STM32F1_FLASH_RETAINED || !clock_ready()) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (expired(deadline)) {
        return NX_ERROR_TIMEOUT;
    }
    if (port->phase != NX_STM32F1_FLASH_IDLE || !locked_and_quiet()) {
        return NX_ERROR_BUSY;
    }
    if (NX_F1_FLASH_BANKS == 2U) {
        port->bank2 = bank2;
    }
    port->phase = NX_STM32F1_FLASH_ACTIVE;
    NX_F1_FLASH_WRITE(NX_F1_FLASH_REG(bank2, KEYR), 0x45670123U);
    NX_F1_FLASH_WRITE(NX_F1_FLASH_REG(bank2, KEYR), 0xcdef89abU);
    nx_arch_dsb();
    if (NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, CR)) != 0U) {
        return finish(port, NX_ERROR_IO, deadline);
    }
    return NX_SUCCESS;
}

static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_stm32f1_flash_state_t* port = context;
    if (!range(port, offset, length) || length == 0U ||
        ((offset | length) & 1U) != 0U || !buffer(port, data, length) ||
        overlaps(data, length, NX_F1_FLASH_MEMORY + offset, length)) {
        return NX_ERROR_INVALID;
    }
    const bool bank2 = bank2_at(offset);
    if (bank2 != bank2_at(offset + (uint32_t)length - 1U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result = begin(port, bank2, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint8_t* input = data;
    for (size_t index = 0U; index < length; index += 2U) {
        if (expired(deadline)) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint16_t value = (uint16_t)((uint16_t)input[index] |
                                    (uint16_t)input[index + 1U] << 8U);
        if (value == UINT16_MAX) {
            continue;
        }
        volatile uint16_t* address =
            (volatile uint16_t*)(NX_F1_FLASH_MEMORY + offset + index);
        /* PM0075, PM0068 and PM0063 §2.3.3 allow a zero pulse on a non-erased
         * halfword.
         */
        if ((*address != UINT16_MAX && value != 0U) || !clock_ready() ||
            !clear_status(bank2) || !control(bank2, FLASH_CR_PG)) {
            result = NX_ERROR_IO;
            break;
        }
        if (expired(deadline)) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        NX_F1_FLASH_STORE(address, value);
        nx_arch_dsb();
        result = pulse(bank2, deadline);
        if (result == NX_SUCCESS && *address != value) {
            result = NX_ERROR_IO;
        }
        if (result != NX_SUCCESS) {
            break;
        }
    }
    return finish(port, result, deadline);
}

static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32f1_flash_state_t* port = context;
    if (!range(port, offset, length) || length == 0U ||
        offset % FLASH_PAGE_SIZE != 0U || length % FLASH_PAGE_SIZE != 0U) {
        return NX_ERROR_INVALID;
    }
    const bool bank2 = bank2_at(offset);
    if (bank2 != bank2_at(offset + (uint32_t)length - 1U)) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_result_t result = begin(port, bank2, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (uint32_t page = offset; page < offset + length;
         page += FLASH_PAGE_SIZE) {
        if (!clock_ready() || !clear_status(bank2) ||
            !control(bank2, FLASH_CR_PER)) {
            result = NX_ERROR_IO;
            break;
        }
        uint32_t address = port->geometry->base_address + page;
        /* FPEC AR and AR2 are write-only; verify the erased page below. */
        NX_F1_FLASH_WRITE(NX_F1_FLASH_REG(bank2, AR), address);
        nx_arch_dsb();
        if (expired(deadline)) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        NX_F1_FLASH_WRITE(NX_F1_FLASH_REG(bank2, CR),
                          FLASH_CR_PER | FLASH_CR_STRT);
        nx_arch_dsb();
        /* The device errata require a separate bus access before reading BSY.
         */
        (void)NX_F1_FLASH_READ(NX_F1_FLASH_REG(bank2, CR));
        nx_arch_dsb();
        result = pulse(bank2, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        for (uint32_t index = 0U; index < FLASH_PAGE_SIZE; ++index) {
            if (NX_F1_FLASH_MEMORY[page + index] != UINT8_MAX) {
                result = NX_ERROR_IO;
                break;
            }
        }
        if (result != NX_SUCCESS || expired(deadline)) {
            break;
        }
    }
    return finish(port, result, deadline);
}

const nx_flash_ops_t nx_stm32f1_flash_ops = {geometry, read, program, erase,
                                             recover};

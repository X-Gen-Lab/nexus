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

static const nx_flash_sector_t s_sectors[256] = {
    {0u, 4096u},       {4096u, 4096u},    {8192u, 4096u},    {12288u, 4096u},
    {16384u, 4096u},   {20480u, 4096u},   {24576u, 4096u},   {28672u, 4096u},
    {32768u, 4096u},   {36864u, 4096u},   {40960u, 4096u},   {45056u, 4096u},
    {49152u, 4096u},   {53248u, 4096u},   {57344u, 4096u},   {61440u, 4096u},
    {65536u, 4096u},   {69632u, 4096u},   {73728u, 4096u},   {77824u, 4096u},
    {81920u, 4096u},   {86016u, 4096u},   {90112u, 4096u},   {94208u, 4096u},
    {98304u, 4096u},   {102400u, 4096u},  {106496u, 4096u},  {110592u, 4096u},
    {114688u, 4096u},  {118784u, 4096u},  {122880u, 4096u},  {126976u, 4096u},
    {131072u, 4096u},  {135168u, 4096u},  {139264u, 4096u},  {143360u, 4096u},
    {147456u, 4096u},  {151552u, 4096u},  {155648u, 4096u},  {159744u, 4096u},
    {163840u, 4096u},  {167936u, 4096u},  {172032u, 4096u},  {176128u, 4096u},
    {180224u, 4096u},  {184320u, 4096u},  {188416u, 4096u},  {192512u, 4096u},
    {196608u, 4096u},  {200704u, 4096u},  {204800u, 4096u},  {208896u, 4096u},
    {212992u, 4096u},  {217088u, 4096u},  {221184u, 4096u},  {225280u, 4096u},
    {229376u, 4096u},  {233472u, 4096u},  {237568u, 4096u},  {241664u, 4096u},
    {245760u, 4096u},  {249856u, 4096u},  {253952u, 4096u},  {258048u, 4096u},
    {262144u, 4096u},  {266240u, 4096u},  {270336u, 4096u},  {274432u, 4096u},
    {278528u, 4096u},  {282624u, 4096u},  {286720u, 4096u},  {290816u, 4096u},
    {294912u, 4096u},  {299008u, 4096u},  {303104u, 4096u},  {307200u, 4096u},
    {311296u, 4096u},  {315392u, 4096u},  {319488u, 4096u},  {323584u, 4096u},
    {327680u, 4096u},  {331776u, 4096u},  {335872u, 4096u},  {339968u, 4096u},
    {344064u, 4096u},  {348160u, 4096u},  {352256u, 4096u},  {356352u, 4096u},
    {360448u, 4096u},  {364544u, 4096u},  {368640u, 4096u},  {372736u, 4096u},
    {376832u, 4096u},  {380928u, 4096u},  {385024u, 4096u},  {389120u, 4096u},
    {393216u, 4096u},  {397312u, 4096u},  {401408u, 4096u},  {405504u, 4096u},
    {409600u, 4096u},  {413696u, 4096u},  {417792u, 4096u},  {421888u, 4096u},
    {425984u, 4096u},  {430080u, 4096u},  {434176u, 4096u},  {438272u, 4096u},
    {442368u, 4096u},  {446464u, 4096u},  {450560u, 4096u},  {454656u, 4096u},
    {458752u, 4096u},  {462848u, 4096u},  {466944u, 4096u},  {471040u, 4096u},
    {475136u, 4096u},  {479232u, 4096u},  {483328u, 4096u},  {487424u, 4096u},
    {491520u, 4096u},  {495616u, 4096u},  {499712u, 4096u},  {503808u, 4096u},
    {507904u, 4096u},  {512000u, 4096u},  {516096u, 4096u},  {520192u, 4096u},
    {524288u, 4096u},  {528384u, 4096u},  {532480u, 4096u},  {536576u, 4096u},
    {540672u, 4096u},  {544768u, 4096u},  {548864u, 4096u},  {552960u, 4096u},
    {557056u, 4096u},  {561152u, 4096u},  {565248u, 4096u},  {569344u, 4096u},
    {573440u, 4096u},  {577536u, 4096u},  {581632u, 4096u},  {585728u, 4096u},
    {589824u, 4096u},  {593920u, 4096u},  {598016u, 4096u},  {602112u, 4096u},
    {606208u, 4096u},  {610304u, 4096u},  {614400u, 4096u},  {618496u, 4096u},
    {622592u, 4096u},  {626688u, 4096u},  {630784u, 4096u},  {634880u, 4096u},
    {638976u, 4096u},  {643072u, 4096u},  {647168u, 4096u},  {651264u, 4096u},
    {655360u, 4096u},  {659456u, 4096u},  {663552u, 4096u},  {667648u, 4096u},
    {671744u, 4096u},  {675840u, 4096u},  {679936u, 4096u},  {684032u, 4096u},
    {688128u, 4096u},  {692224u, 4096u},  {696320u, 4096u},  {700416u, 4096u},
    {704512u, 4096u},  {708608u, 4096u},  {712704u, 4096u},  {716800u, 4096u},
    {720896u, 4096u},  {724992u, 4096u},  {729088u, 4096u},  {733184u, 4096u},
    {737280u, 4096u},  {741376u, 4096u},  {745472u, 4096u},  {749568u, 4096u},
    {753664u, 4096u},  {757760u, 4096u},  {761856u, 4096u},  {765952u, 4096u},
    {770048u, 4096u},  {774144u, 4096u},  {778240u, 4096u},  {782336u, 4096u},
    {786432u, 4096u},  {790528u, 4096u},  {794624u, 4096u},  {798720u, 4096u},
    {802816u, 4096u},  {806912u, 4096u},  {811008u, 4096u},  {815104u, 4096u},
    {819200u, 4096u},  {823296u, 4096u},  {827392u, 4096u},  {831488u, 4096u},
    {835584u, 4096u},  {839680u, 4096u},  {843776u, 4096u},  {847872u, 4096u},
    {851968u, 4096u},  {856064u, 4096u},  {860160u, 4096u},  {864256u, 4096u},
    {868352u, 4096u},  {872448u, 4096u},  {876544u, 4096u},  {880640u, 4096u},
    {884736u, 4096u},  {888832u, 4096u},  {892928u, 4096u},  {897024u, 4096u},
    {901120u, 4096u},  {905216u, 4096u},  {909312u, 4096u},  {913408u, 4096u},
    {917504u, 4096u},  {921600u, 4096u},  {925696u, 4096u},  {929792u, 4096u},
    {933888u, 4096u},  {937984u, 4096u},  {942080u, 4096u},  {946176u, 4096u},
    {950272u, 4096u},  {954368u, 4096u},  {958464u, 4096u},  {962560u, 4096u},
    {966656u, 4096u},  {970752u, 4096u},  {974848u, 4096u},  {978944u, 4096u},
    {983040u, 4096u},  {987136u, 4096u},  {991232u, 4096u},  {995328u, 4096u},
    {999424u, 4096u},  {1003520u, 4096u}, {1007616u, 4096u}, {1011712u, 4096u},
    {1015808u, 4096u}, {1019904u, 4096u}, {1024000u, 4096u}, {1028096u, 4096u},
    {1032192u, 4096u}, {1036288u, 4096u}, {1040384u, 4096u}, {1044480u, 4096u},
};
static const nx_flash_geometry_t s_geometry = {0x08000000u, 1048576u, 2u,
                                               s_sectors,   256u,     0u};

/** \brief           Validate physical offsets without addition overflow. */
static bool valid_range(uint32_t offset, size_t length) {
    return offset <= s_geometry.size && length <= s_geometry.size - offset;
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
    return port && port == s_flash && port->initialized ? &s_geometry : NULL;
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
        memcpy(data, (const void*)(uintptr_t)(s_geometry.base_address + offset),
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
        uint32_t address = s_geometry.base_address + offset + (uint32_t)i;
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
        uint32_t address = s_geometry.base_address + offset + (uint32_t)i;
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

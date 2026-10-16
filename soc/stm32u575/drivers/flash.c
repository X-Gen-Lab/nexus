/**
 * \file            flash.c
 * \brief           Exact U575 physical pages and settled128bit Flash pulses
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_provider.h"
#include <string.h>
#define FLASH_BYTES 2097152U
#define PAGE_BYTES  8192U
#define FLASH_ERRORS                                                           \
    ((uint32_t)(FLASH_NSSR_OPERR | FLASH_NSSR_PROGERR | FLASH_NSSR_WRPERR |    \
                FLASH_NSSR_PGAERR | FLASH_NSSR_SIZERR | FLASH_NSSR_PGSERR |    \
                FLASH_NSSR_OPTWERR))
#define PULSE_BUSY      ((uint32_t)(FLASH_NSSR_BSY | FLASH_NSSR_WDW))
#define CONTROL_ALLOWED ((uint32_t)(FLASH_NSCR_LOCK | FLASH_NSCR_OPTLOCK))
static const nx_flash_sector_t sectors[] = {
    {0U, 8192U},       {8192U, 8192U},    {16384U, 8192U},   {24576U, 8192U},
    {32768U, 8192U},   {40960U, 8192U},   {49152U, 8192U},   {57344U, 8192U},
    {65536U, 8192U},   {73728U, 8192U},   {81920U, 8192U},   {90112U, 8192U},
    {98304U, 8192U},   {106496U, 8192U},  {114688U, 8192U},  {122880U, 8192U},
    {131072U, 8192U},  {139264U, 8192U},  {147456U, 8192U},  {155648U, 8192U},
    {163840U, 8192U},  {172032U, 8192U},  {180224U, 8192U},  {188416U, 8192U},
    {196608U, 8192U},  {204800U, 8192U},  {212992U, 8192U},  {221184U, 8192U},
    {229376U, 8192U},  {237568U, 8192U},  {245760U, 8192U},  {253952U, 8192U},
    {262144U, 8192U},  {270336U, 8192U},  {278528U, 8192U},  {286720U, 8192U},
    {294912U, 8192U},  {303104U, 8192U},  {311296U, 8192U},  {319488U, 8192U},
    {327680U, 8192U},  {335872U, 8192U},  {344064U, 8192U},  {352256U, 8192U},
    {360448U, 8192U},  {368640U, 8192U},  {376832U, 8192U},  {385024U, 8192U},
    {393216U, 8192U},  {401408U, 8192U},  {409600U, 8192U},  {417792U, 8192U},
    {425984U, 8192U},  {434176U, 8192U},  {442368U, 8192U},  {450560U, 8192U},
    {458752U, 8192U},  {466944U, 8192U},  {475136U, 8192U},  {483328U, 8192U},
    {491520U, 8192U},  {499712U, 8192U},  {507904U, 8192U},  {516096U, 8192U},
    {524288U, 8192U},  {532480U, 8192U},  {540672U, 8192U},  {548864U, 8192U},
    {557056U, 8192U},  {565248U, 8192U},  {573440U, 8192U},  {581632U, 8192U},
    {589824U, 8192U},  {598016U, 8192U},  {606208U, 8192U},  {614400U, 8192U},
    {622592U, 8192U},  {630784U, 8192U},  {638976U, 8192U},  {647168U, 8192U},
    {655360U, 8192U},  {663552U, 8192U},  {671744U, 8192U},  {679936U, 8192U},
    {688128U, 8192U},  {696320U, 8192U},  {704512U, 8192U},  {712704U, 8192U},
    {720896U, 8192U},  {729088U, 8192U},  {737280U, 8192U},  {745472U, 8192U},
    {753664U, 8192U},  {761856U, 8192U},  {770048U, 8192U},  {778240U, 8192U},
    {786432U, 8192U},  {794624U, 8192U},  {802816U, 8192U},  {811008U, 8192U},
    {819200U, 8192U},  {827392U, 8192U},  {835584U, 8192U},  {843776U, 8192U},
    {851968U, 8192U},  {860160U, 8192U},  {868352U, 8192U},  {876544U, 8192U},
    {884736U, 8192U},  {892928U, 8192U},  {901120U, 8192U},  {909312U, 8192U},
    {917504U, 8192U},  {925696U, 8192U},  {933888U, 8192U},  {942080U, 8192U},
    {950272U, 8192U},  {958464U, 8192U},  {966656U, 8192U},  {974848U, 8192U},
    {983040U, 8192U},  {991232U, 8192U},  {999424U, 8192U},  {1007616U, 8192U},
    {1015808U, 8192U}, {1024000U, 8192U}, {1032192U, 8192U}, {1040384U, 8192U},
    {1048576U, 8192U}, {1056768U, 8192U}, {1064960U, 8192U}, {1073152U, 8192U},
    {1081344U, 8192U}, {1089536U, 8192U}, {1097728U, 8192U}, {1105920U, 8192U},
    {1114112U, 8192U}, {1122304U, 8192U}, {1130496U, 8192U}, {1138688U, 8192U},
    {1146880U, 8192U}, {1155072U, 8192U}, {1163264U, 8192U}, {1171456U, 8192U},
    {1179648U, 8192U}, {1187840U, 8192U}, {1196032U, 8192U}, {1204224U, 8192U},
    {1212416U, 8192U}, {1220608U, 8192U}, {1228800U, 8192U}, {1236992U, 8192U},
    {1245184U, 8192U}, {1253376U, 8192U}, {1261568U, 8192U}, {1269760U, 8192U},
    {1277952U, 8192U}, {1286144U, 8192U}, {1294336U, 8192U}, {1302528U, 8192U},
    {1310720U, 8192U}, {1318912U, 8192U}, {1327104U, 8192U}, {1335296U, 8192U},
    {1343488U, 8192U}, {1351680U, 8192U}, {1359872U, 8192U}, {1368064U, 8192U},
    {1376256U, 8192U}, {1384448U, 8192U}, {1392640U, 8192U}, {1400832U, 8192U},
    {1409024U, 8192U}, {1417216U, 8192U}, {1425408U, 8192U}, {1433600U, 8192U},
    {1441792U, 8192U}, {1449984U, 8192U}, {1458176U, 8192U}, {1466368U, 8192U},
    {1474560U, 8192U}, {1482752U, 8192U}, {1490944U, 8192U}, {1499136U, 8192U},
    {1507328U, 8192U}, {1515520U, 8192U}, {1523712U, 8192U}, {1531904U, 8192U},
    {1540096U, 8192U}, {1548288U, 8192U}, {1556480U, 8192U}, {1564672U, 8192U},
    {1572864U, 8192U}, {1581056U, 8192U}, {1589248U, 8192U}, {1597440U, 8192U},
    {1605632U, 8192U}, {1613824U, 8192U}, {1622016U, 8192U}, {1630208U, 8192U},
    {1638400U, 8192U}, {1646592U, 8192U}, {1654784U, 8192U}, {1662976U, 8192U},
    {1671168U, 8192U}, {1679360U, 8192U}, {1687552U, 8192U}, {1695744U, 8192U},
    {1703936U, 8192U}, {1712128U, 8192U}, {1720320U, 8192U}, {1728512U, 8192U},
    {1736704U, 8192U}, {1744896U, 8192U}, {1753088U, 8192U}, {1761280U, 8192U},
    {1769472U, 8192U}, {1777664U, 8192U}, {1785856U, 8192U}, {1794048U, 8192U},
    {1802240U, 8192U}, {1810432U, 8192U}, {1818624U, 8192U}, {1826816U, 8192U},
    {1835008U, 8192U}, {1843200U, 8192U}, {1851392U, 8192U}, {1859584U, 8192U},
    {1867776U, 8192U}, {1875968U, 8192U}, {1884160U, 8192U}, {1892352U, 8192U},
    {1900544U, 8192U}, {1908736U, 8192U}, {1916928U, 8192U}, {1925120U, 8192U},
    {1933312U, 8192U}, {1941504U, 8192U}, {1949696U, 8192U}, {1957888U, 8192U},
    {1966080U, 8192U}, {1974272U, 8192U}, {1982464U, 8192U}, {1990656U, 8192U},
    {1998848U, 8192U}, {2007040U, 8192U}, {2015232U, 8192U}, {2023424U, 8192U},
    {2031616U, 8192U}, {2039808U, 8192U}, {2048000U, 8192U}, {2056192U, 8192U},
    {2064384U, 8192U}, {2072576U, 8192U}, {2080768U, 8192U}, {2088960U, 8192U},
};
const nx_flash_geometry_t g_nx_stm32u575_flash_geometry = {
    .base_address = 0x08000000U,
    .size = FLASH_BYTES,
    .program_unit = 16U,
    .sectors = sectors,
    .sector_count = 256U,
    .program_flags = NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE};
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32u575_flash_state_t* p) {
    return p != NULL && p->registers != NULL && p->memory != NULL &&
           (uintptr_t)p->memory % 16U == 0U;
}
static bool range(const nx_stm32u575_flash_state_t* p, uint32_t offset,
                  size_t length) {
    return valid(p) && offset <= FLASH_BYTES && length <= FLASH_BYTES - offset;
}
static bool span(const void* pointer, size_t bytes) {
    return pointer != NULL && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
static bool external(const nx_stm32u575_flash_state_t* p, const void* data,
                     size_t bytes) {
    return span(data, bytes) && !overlap(data, bytes, p, sizeof(*p)) &&
           !overlap(data, bytes, p->registers, sizeof(*p->registers));
}
static nx_result_t admission(const nx_stm32u575_flash_state_t* p) {
    if (!g_nx_stm32u575_system.started ||
        g_nx_stm32u575_system.core_hz != 4000000U)
        return NX_ERROR_STATE;
    nx_result_t identity = nx_stm32u575_validate_identity();
    if (identity != NX_SUCCESS)
        return identity;
    if ((NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U ||
        (NX_U575_READ(&p->registers->NSCR) & ~CONTROL_ALLOWED) != 0U ||
        (NX_U575_READ(&p->registers->NSCR) & FLASH_NSCR_LOCK) == 0U ||
        !NX_U575_IRQ_AVAILABLE(FLASH_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_flash_initialize(nx_stm32u575_flash_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->initialized || p->retained || p->active)
        return NX_ERROR_BUSY;
    nx_result_t result = admission(p);
    if (result == NX_SUCCESS)
        p->initialized = p->retained = true;
    return result;
}
static const nx_flash_geometry_t* geometry(const void* context) {
    return context != NULL ? &g_nx_stm32u575_flash_geometry : NULL;
}
static nx_result_t read(const void* context, uint32_t offset, void* data,
                        size_t length) {
    const nx_stm32u575_flash_state_t* p = context;
    if (!range(p, offset, length) ||
        (length != 0U &&
         (!external(p, data, length) ||
          overlap(data, length, (const void*)(p->memory + offset), length))))
        return NX_ERROR_INVALID;
    if (!p->initialized)
        return NX_ERROR_STATE;
    if (p->active || (NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U)
        return NX_ERROR_BUSY;
    uint8_t* output = data;
    for (size_t index = 0U; index < length; ++index)
        output[index] = p->memory[offset + index];
    return NX_SUCCESS;
}
static nx_result_t begin(nx_stm32u575_flash_state_t* p, nx_time_us_t deadline) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!p->initialized)
        return NX_ERROR_STATE;
    if (p->active)
        return NX_ERROR_BUSY;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_result_t result = admission(p);
    if (result != NX_SUCCESS)
        return result;
    NX_U575_WRITE(&p->registers->NSSR, FLASH_ERRORS | FLASH_NSSR_EOP);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->registers->NSSR) & (FLASH_ERRORS | FLASH_NSSR_EOP)) !=
        0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(&p->registers->NSKEYR, 0x45670123U);
    NX_U575_WRITE(&p->registers->NSKEYR, 0xcdef89abU);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->registers->NSCR) & FLASH_NSCR_LOCK) != 0U)
        return NX_ERROR_IO;
    p->active = true;
    return NX_SUCCESS;
}
/** \brief A deadline never interrupts a physical program/erase pulse. */
static nx_result_t pulse(nx_stm32u575_flash_state_t* p, nx_time_us_t deadline) {
    do {
        NX_U575_FLASH_POLL(p);
    } while ((NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U);
    uint32_t status = NX_U575_READ(&p->registers->NSSR);
    if ((status & FLASH_ERRORS) != 0U || (status & FLASH_NSSR_EOP) == 0U)
        return NX_ERROR_IO;
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static nx_result_t end(nx_stm32u575_flash_state_t* p) {
    uint32_t control = NX_U575_READ(&p->registers->NSCR);
    control &= CONTROL_ALLOWED;
    control |= FLASH_NSCR_LOCK;
    NX_U575_WRITE(&p->registers->NSCR, control);
    nx_arch_dsb();
    p->active = false;
    if (NX_U575_READ(&p->registers->NSCR) != control) {
        p->initialized = false;
        return NX_ERROR_IO;
    }
    /* RM0456 7.9.7 gates EOP itself with EOPIE. The exclusive FLASH IRQ
     * remains disabled throughout polling; withdraw its source and lock
     * before clearing the pending fact, preserving its original priority. */
    nx_result_t result = NX_U575_IRQ_CONTROL(FLASH_IRQn, false, 0U);
    if (result != NX_SUCCESS || !NX_U575_IRQ_AVAILABLE(FLASH_IRQn)) {
        p->initialized = false;
        return result == NX_SUCCESS ? NX_ERROR_IO : result;
    }
    return NX_SUCCESS;
}
static nx_result_t program(void* context, uint32_t offset, const void* data,
                           size_t length, nx_time_us_t deadline) {
    nx_stm32u575_flash_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!range(p, offset, length) || length == 0U || offset % 16U != 0U ||
        length % 16U != 0U || !external(p, data, length) ||
        overlap(data, length, (const void*)(p->memory + offset), length))
        return NX_ERROR_INVALID;
    /* Reject known programmed cells before unlocking. All-FF reads cannot
     * prove hidden ECC history: the caller must own a fresh sector erase
     * and must erase again after every failed or interrupted pulse. */
    for (size_t index = 0U; index < length; ++index)
        if (p->memory[offset + index] != 0xffU)
            return NX_ERROR_STATE;
    nx_result_t result = begin(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    const uint8_t* input = data;
    for (size_t index = 0U; index < length; index += 16U) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t words[4];
        memcpy(words, input + index, sizeof(words));
        /* Writing all ones still programs ECC; preserve the erased physical
         * unit by issuing no programming pulse for an all-FF input unit. */
        if (words[0] == UINT32_MAX && words[1] == UINT32_MAX &&
            words[2] == UINT32_MAX && words[3] == UINT32_MAX)
            continue;
        NX_U575_WRITE(&p->registers->NSSR, FLASH_ERRORS | FLASH_NSSR_EOP);
        nx_arch_dsb();
        if ((NX_U575_READ(&p->registers->NSSR) &
             (FLASH_ERRORS | FLASH_NSSR_EOP)) != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        uint32_t control = NX_U575_READ(&p->registers->NSCR) | FLASH_NSCR_PG |
                           FLASH_NSCR_EOPIE;
        NX_U575_WRITE(&p->registers->NSCR, control);
        nx_arch_dsb();
        if (NX_U575_READ(&p->registers->NSCR) != control) {
            result = NX_ERROR_IO;
            break;
        }
        /* RM0456 7.3.7 requires four successive32bit writes in one aligned
         * quad-word. Input data is copied before the first write; the brief
         * incoming-mask-preserving exclusion cannot interleave another ISR. */
        nx_arch_irq_state_t saved = nx_arch_irq_save();
        for (unsigned word = 0U; word < 4U; ++word)
            NX_U575_FLASH_PROGRAM32(p->memory + offset + index + word * 4U,
                                    words[word]);
        nx_arch_dsb();
        nx_arch_irq_restore(saved);
        result = pulse(p, deadline);
        if (result != NX_SUCCESS)
            break;
        for (unsigned byte = 0U; byte < 16U; ++byte)
            if (p->memory[offset + index + byte] != input[index + byte])
                result = NX_ERROR_IO;
        if (result != NX_SUCCESS)
            break;
    }
    nx_result_t cleanup = end(p);
    return cleanup == NX_SUCCESS ? result : cleanup;
}
static nx_result_t erase(void* context, uint32_t offset, size_t length,
                         nx_time_us_t deadline) {
    nx_stm32u575_flash_state_t* p = context;
    if (!range(p, offset, length) || length == 0U ||
        offset % PAGE_BYTES != 0U || length % PAGE_BYTES != 0U)
        return NX_ERROR_INVALID;
    nx_result_t result = begin(p, deadline);
    if (result != NX_SUCCESS)
        return result;
    for (size_t index = 0U; index < length; index += PAGE_BYTES) {
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            result = NX_ERROR_TIMEOUT;
            break;
        }
        uint32_t page = (offset + (uint32_t)index) / PAGE_BYTES;
        uint32_t control =
            NX_U575_READ(&p->registers->NSCR) & FLASH_NSCR_OPTLOCK;
        control |= FLASH_NSCR_PER | FLASH_NSCR_EOPIE |
                   ((page % 128U) << FLASH_NSCR_PNB_Pos);
        if (page >= 128U)
            control |= FLASH_NSCR_BKER;
        NX_U575_WRITE(&p->registers->NSSR, FLASH_ERRORS | FLASH_NSSR_EOP);
        nx_arch_dsb();
        if ((NX_U575_READ(&p->registers->NSSR) &
             (FLASH_ERRORS | FLASH_NSSR_EOP)) != 0U) {
            result = NX_ERROR_IO;
            break;
        }
        NX_U575_WRITE(&p->registers->NSCR, control);
        nx_arch_dsb();
        if (NX_U575_READ(&p->registers->NSCR) != control) {
            result = NX_ERROR_IO;
            break;
        }
        NX_U575_WRITE(&p->registers->NSCR, control | FLASH_NSCR_STRT);
        result = pulse(p, deadline);
        if (result != NX_SUCCESS)
            break;
    }
    nx_result_t cleanup = end(p);
    return cleanup == NX_SUCCESS ? result : cleanup;
}
nx_result_t nx_stm32u575_flash_close(nx_stm32u575_flash_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->active || (NX_U575_READ(&p->registers->NSSR) & PULSE_BUSY) != 0U)
        return NX_ERROR_BUSY;
    if (!p->retained)
        return NX_SUCCESS;
    nx_result_t result = end(p);
    if (result == NX_SUCCESS)
        p->initialized = p->retained = false;
    return result;
}
const nx_flash_ops_t nx_stm32u575_flash_ops = {
    .geometry = geometry, .read = read, .program = program, .erase = erase};

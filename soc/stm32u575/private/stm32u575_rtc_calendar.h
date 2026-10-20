/**
 * \file            stm32u575_rtc_calendar.h
 * \brief           Shared exact U575 BCD bypass tuple decoding
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_RTC_CALENDAR_H
#define NX_STM32U575_RTC_CALENDAR_H
#include "rtc_lptim_internal.h"
#include "stm32u575_system.h"
/** \brief Calendar and subsecond copied only after a complete coherent pair.
 * \note Callers establish the exact BCD24h/BYPSHAD/PRER127/255 mode and
 *       separate valid output storage before invoking this private reader.
 */
typedef struct {
    nx_rtc_calendar_t calendar;
    uint16_t subsecond;
} nx_stm32u575_rtc_tuple_t;
/** \brief Decode each BCD digit before calendar range validation. */
static inline bool nx_stm32u575_rtc_bcd(uint32_t value, uint8_t* out) {
    if ((value & 15U) > 9U || (value >> 4U) > 9U)
        return false;
    *out = (uint8_t)((value >> 4U) * 10U + (value & 15U));
    return true;
}
/** \brief Conservative selected MSIS4 access profile, not an RTC-write rule.
 * \note RM0456 Rev7 63.3.12 discusses seven-times clocking in shadow mode.
 *       This BYPSHAD reader instead repeats complete tuples; its exact selected
 *       4MHz/equal-bus profile also keeps APB well above that ratio.
 */
static inline bool nx_stm32u575_rtc_access(RCC_TypeDef* r) {
    uint32_t selection = NX_U575_READ(&r->ICSCR1);
    uint32_t range =
        (selection & RCC_ICSCR1_MSIRGSEL) == 0U
            ? (NX_U575_READ(&r->CSR) & RCC_CSR_MSISSRANGE) >>
                  RCC_CSR_MSISSRANGE_Pos
            : (selection & RCC_ICSCR1_MSISRANGE) >> RCC_ICSCR1_MSISRANGE_Pos;
    uint32_t cr = NX_U575_READ(&r->CR);
    uint32_t allowed =
        RCC_CR_MSISON | RCC_CR_MSISRDY | RCC_CR_MSIKON | RCC_CR_MSIKRDY;
    return (NX_U575_READ(&r->APB3ENR) & RCC_APB3ENR_RTCAPBEN) != 0U &&
           range == 4U && (cr & ~allowed) == 0U &&
           (cr & (RCC_CR_MSISON | RCC_CR_MSISRDY)) ==
               (RCC_CR_MSISON | RCC_CR_MSISRDY) &&
           NX_U575_READ(&r->CFGR1) == 0U && NX_U575_READ(&r->CFGR2) == 0U &&
           NX_U575_READ(&r->CFGR3) == 0U;
}
/** \brief ES0499 Rev12 2.19.3: bypass shadows and repeat complete tuples. */
static inline nx_result_t
nx_stm32u575_rtc_tuple_read(RTC_TypeDef* p, RCC_TypeDef* r,
                            nx_stm32u575_rtc_tuple_t* out) {
    for (unsigned n = 0U; n < 8U; ++n) {
        if (!nx_stm32u575_rtc_access(r))
            return NX_ERROR_STATE;
        uint32_t subsecond = NX_U575_READ(&p->SSR);
        uint32_t time = NX_U575_READ(&p->TR);
        uint32_t date = NX_U575_READ(&p->DR);
        uint32_t next_subsecond = NX_U575_READ(&p->SSR);
        uint32_t next_time = NX_U575_READ(&p->TR);
        uint32_t next_date = NX_U575_READ(&p->DR);
        if (subsecond != next_subsecond || time != next_time ||
            date != next_date)
            continue;
        nx_rtc_calendar_t value = {0};
        value.weekday = (uint8_t)((date >> 13U) & 7U);
        if (subsecond > 255U || (time & ~UINT32_C(0x003f7f7f)) != 0U ||
            (date & ~UINT32_C(0x00ffff3f)) != 0U ||
            !nx_stm32u575_rtc_bcd((time >> 16U) & 63U, &value.hour) ||
            !nx_stm32u575_rtc_bcd((time >> 8U) & 127U, &value.minute) ||
            !nx_stm32u575_rtc_bcd(time & 127U, &value.second) ||
            !nx_stm32u575_rtc_bcd((date >> 16U) & 255U, &value.year) ||
            !nx_stm32u575_rtc_bcd((date >> 8U) & 31U, &value.month) ||
            !nx_stm32u575_rtc_bcd(date & 63U, &value.day) ||
            nx_rtc_calendar_validate(&value) != NX_SUCCESS)
            return NX_ERROR_STATE;
        out->calendar = value;
        out->subsecond = (uint16_t)subsecond;
        return NX_SUCCESS;
    }
    return NX_ERROR_BUSY;
}
#endif

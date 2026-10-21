/**
 * \file            model.cpp
 * \brief           Independent U575 SRAM4/LPTIM1 request and bus behavior
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
LpdmaModel model;
static constexpr unsigned irq_word = 114U / 32U;
static constexpr uint32_t irq_bit = 1U << (114U % 32U);
static bool dma_address(uintptr_t address) {
    return address >= LPDMA1_BASE_NS && address < LPDMA1_BASE_NS + 0x400U;
}
static bool sram4_span(uintptr_t address, size_t amount) {
    return address >= SRAM4_BASE_NS && amount <= SRAM4_SIZE &&
           address - SRAM4_BASE_NS <= SRAM4_SIZE - amount;
}
static bool lptim_destination(uintptr_t address) {
    return address == reinterpret_cast<uintptr_t>(&LPTIM1->CCR1) ||
           address == reinterpret_cast<uintptr_t>(&LPTIM1->RCR) ||
           address == reinterpret_cast<uintptr_t>(&LPTIM1->ARR);
}
void LpdmaModel::reset() {
    *this = {};
    reg(&RCC->CR) = RCC_CR_MSISON | RCC_CR_MSISRDY;
    reg(&RCC->ICSCR1) = RCC_ICSCR1_MSIRGSEL | (4U << RCC_ICSCR1_MSISRANGE_Pos) |
                        (4U << RCC_ICSCR1_MSIKRANGE_Pos);
    reg(&RCC->AHB3ENR) =
        RCC_AHB3ENR_PWREN | RCC_AHB3ENR_SRAM4EN | RCC_AHB3ENR_LPDMA1EN;
    reg(&RCC->APB3ENR) = RCC_APB3ENR_RTCAPBEN;
    reg(&PWR->VOSR) = PWR_VOSR_VOSRDY; /* VOS00 is Range4, RM10.10.4. */
    reg(&RTC->CR) = RTC_CR_BYPSHAD;
    reg(&LPTIM1->ARR) = 1U;
    for (auto* channel :
         {LPDMA1_Channel0, LPDMA1_Channel1, LPDMA1_Channel2, LPDMA1_Channel3})
        reg(&channel->CSR) = DMA_CSR_IDLEF;
}
void LpdmaModel::lptim_event() {
    auto& channel = *LPDMA1_Channel0;
    if ((reg(&LPTIM1->CR) & LPTIM_CR_ENABLE) == 0U)
        return;
    reg(&LPTIM1->ISR) |= LPTIM_ISR_UE;
    if ((reg(&LPTIM1->DIER) & LPTIM_DIER_UEDE) == 0U ||
        (reg(&LPTIM1->CCMR1) & LPTIM_CCMR1_CC1E) == 0U ||
        (reg(&channel.CCR) & DMA_CCR_EN) == 0U || suspended)
        return;
    constexpr uint32_t hardware_request = 13U | DMA_CTR2_BREQ;
    if ((reg(&channel.CTR2) &
         (DMA_CTR2_REQSEL | DMA_CTR2_SWREQ | DMA_CTR2_BREQ | DMA_CTR2_DREQ |
          DMA_CTR2_TRIGPOL)) != hardware_request)
        throw std::runtime_error("model expected LPTIM1 request13 BREQ1");
    const uintptr_t destinations[] = {
        reinterpret_cast<uintptr_t>(&LPTIM1->CCR1),
        reinterpret_cast<uintptr_t>(&LPTIM1->RCR),
        reinterpret_cast<uintptr_t>(&LPTIM1->ARR)};
    for (unsigned n = 0U; n < 3U; ++n) {
        if ((reg(&channel.CCR) & DMA_CCR_EN) == 0U ||
            reg(&channel.CBR1) != sizeof(uint32_t) ||
            reg(&channel.CDAR) != destinations[n])
            throw std::runtime_error("model expected fixed CCR1/RCR/ARR tuple");
        if (n != 0U && (reg(&channel.CTR2) & DMA_CTR2_SWREQ) == 0U)
            throw std::runtime_error("model expected SWREQ continuation");
        finish_block();
    }
    if ((reg(&channel.CCR) & DMA_CCR_EN) != 0U || reg(&channel.CLLR) != 0U)
        throw std::runtime_error("model expected one finite LPTIM1 tuple");
}
extern "C" uint32_t nx_u575_lpdma_copy_read(uintptr_t address) {
    ++model.reads;
    if (dma_address(address))
        ++model.dma_reads;
    return model.values[address];
}
extern "C" void nx_u575_lpdma_copy_write(uintptr_t address, uint32_t value) {
    ++model.writes;
    if (dma_address(address))
        ++model.dma_writes;
    auto& channel = *LPDMA1_Channel0;
    if (address == reinterpret_cast<uintptr_t>(&LPDMA1->PRIVCFGR) &&
        model.hold_privilege)
        return;
    if (address == reinterpret_cast<uintptr_t>(&channel.CCR)) {
        uint32_t before = model.reg(&channel.CCR);
        uint32_t status = model.reg(&channel.CSR);
        if ((value & DMA_CCR_RESET) != 0U) {
            if (!model.hold_reset && (status & DMA_CSR_IDLEF) != 0U &&
                ((before & DMA_CCR_EN) == 0U ||
                 (status & DMA_CSR_SUSPF) != 0U)) {
                ++model.resets;
                model.reg(&channel.CCR) =
                    value & ~static_cast<uint32_t>(DMA_CCR_EN | DMA_CCR_SUSP |
                                                   DMA_CCR_RESET);
                model.triggered = false;
                model.pending_hit = false;
                model.link_pending = false;
                model.suspended = false;
                model.reg(&channel.CSR) |= DMA_CSR_IDLEF;
            }
            return;
        }
        /* RM18.8.9: clearing EN is ignored. */
        model.reg(&channel.CCR) = value | (before & DMA_CCR_EN);
        if ((value & DMA_CCR_SUSP) != 0U && (before & DMA_CCR_EN) != 0U &&
            !model.hold_suspend && !model.link_pending) {
            model.reg(&channel.CSR) |= DMA_CSR_SUSPF | DMA_CSR_IDLEF;
            model.suspended = true;
        }
        if ((value & DMA_CCR_EN) != 0U && (before & DMA_CCR_EN) == 0U)
            model.reg(&channel.CSR) &=
                ~static_cast<uint32_t>(DMA_CSR_IDLEF | DMA_CSR_SUSPF);
        return;
    }
    if (address == reinterpret_cast<uintptr_t>(&channel.CFCR)) {
        if (!model.hold_clear)
            model.reg(&channel.CSR) &= ~value;
        return;
    }
    if (address == reinterpret_cast<uintptr_t>(&NVIC->ISER[irq_word])) {
        model.reg(&NVIC->ISER[irq_word]) |= value;
        return;
    }
    if (address == reinterpret_cast<uintptr_t>(&NVIC->ICER[irq_word])) {
        model.reg(&NVIC->ISER[irq_word]) &= ~value;
        return;
    }
    if (address == reinterpret_cast<uintptr_t>(&NVIC->ICPR[irq_word])) {
        if (!model.hold_irq)
            model.reg(&NVIC->ISPR[irq_word]) &= ~value;
        return;
    }
    model.values[address] = value;
}
void LpdmaModel::trigger() {
    auto& channel = *LPDMA1_Channel0;
    if ((reg(&channel.CCR) & DMA_CCR_EN) == 0U)
        return;
    uint32_t configuration = reg(&channel.CTR2);
    if (((configuration & DMA_CTR2_TRIGSEL) >> DMA_CTR2_TRIGSEL_Pos) != 14U ||
        (configuration & DMA_CTR2_TRIGPOL) == 0U)
        return;
    if (triggered && pending_hit)
        reg(&channel.CSR) |= DMA_CSR_TOF;
    else if (triggered)
        pending_hit = true;
    triggered = true;
}
void LpdmaModel::transfer(size_t bytes) {
    auto& channel = *LPDMA1_Channel0;
    if ((reg(&channel.CCR) & DMA_CCR_EN) == 0U || suspended ||
        ((reg(&channel.CTR2) & DMA_CTR2_TRIGPOL) != 0U && !triggered))
        return;
    size_t amount = std::min(bytes, static_cast<size_t>(reg(&channel.CBR1)));
    auto source = static_cast<uintptr_t>(reg(&channel.CSAR));
    auto destination = static_cast<uintptr_t>(reg(&channel.CDAR));
    if (lptim_destination(destination)) {
        constexpr uint32_t widths = DMA_CTR1_SDW_LOG2_1 | DMA_CTR1_DDW_LOG2_1;
        if (amount != sizeof(uint32_t) ||
            !sram4_span(source, sizeof(uint32_t)) || (source & 3U) != 0U ||
            (reg(&channel.CTR1) &
             (DMA_CTR1_SDW_LOG2 | DMA_CTR1_DDW_LOG2 | DMA_CTR1_DINC)) != widths)
            throw std::runtime_error("model expected one fixed word DMA write");
        uint32_t value;
        std::memcpy(&value, reinterpret_cast<const void*>(source),
                    sizeof(value));
        lptim_writes.emplace_back(destination, value);
        values[destination] = value;
        if (!hold_write_ok) {
            uint32_t flag =
                destination == reinterpret_cast<uintptr_t>(&LPTIM1->CCR1)
                    ? LPTIM_ISR_CMP1OK
                    : (destination == reinterpret_cast<uintptr_t>(&LPTIM1->RCR)
                           ? LPTIM_ISR_REPOK
                           : LPTIM_ISR_ARROK);
            reg(&LPTIM1->ISR) |= flag;
        }
        if (destination == reinterpret_cast<uintptr_t>(&LPTIM1->ARR))
            reg(&LPTIM1->ISR) &= ~static_cast<uint32_t>(LPTIM_ISR_UE);
        if ((reg(&channel.CTR1) & DMA_CTR1_SINC) != 0U)
            reg(&channel.CSAR) += static_cast<uint32_t>(sizeof(uint32_t));
        reg(&channel.CBR1) = 0U;
        nx_u575_lptim1_dma_write(destination, value);
        return;
    }
    if (source < SRAM4_BASE_NS ||
        source + amount > SRAM4_BASE_NS + SRAM4_SIZE ||
        destination < SRAM4_BASE_NS ||
        destination + amount > SRAM4_BASE_NS + SRAM4_SIZE)
        throw std::runtime_error(
            "model DMA bus address outside physical SRAM4");
    std::memcpy(reinterpret_cast<void*>(destination),
                reinterpret_cast<void*>(source), amount);
    reg(&channel.CSAR) += static_cast<uint32_t>(amount);
    reg(&channel.CDAR) += static_cast<uint32_t>(amount);
    reg(&channel.CBR1) -= static_cast<uint32_t>(amount);
}
void LpdmaModel::finish_block() {
    auto& channel = *LPDMA1_Channel0;
    transfer(reg(&channel.CBR1));
    if (reg(&channel.CBR1) != 0U)
        return;
    uint32_t link = reg(&channel.CLLR);
    uint32_t event_mode = reg(&channel.CTR2) & DMA_CTR2_TCEM;
    if (event_mode == 0U || event_mode == DMA_CTR2_TCEM_0)
        reg(&channel.CSR) |= DMA_CSR_TCF;
    if (link != 0U) {
        auto address =
            static_cast<uintptr_t>(reg(&channel.CLBAR) | (link & DMA_CLLR_LA));
        unsigned words_in_link = 0U;
        const uint32_t update_flags[] = {DMA_CLLR_UT1, DMA_CLLR_UT2,
                                         DMA_CLLR_UB1, DMA_CLLR_USA,
                                         DMA_CLLR_UDA, DMA_CLLR_ULL};
        for (uint32_t flag : update_flags)
            if ((link & flag) != 0U)
                ++words_in_link;
        if (!sram4_span(address, words_in_link * sizeof(uint32_t)))
            throw std::runtime_error("model linked-list read outside SRAM4");
        const auto* words = reinterpret_cast<const uint32_t*>(address);
        const uint32_t flags[] = {DMA_CLLR_UT1, DMA_CLLR_UT2, DMA_CLLR_UB1,
                                  DMA_CLLR_USA, DMA_CLLR_UDA, DMA_CLLR_ULL};
        volatile uint32_t* registers[] = {&channel.CTR1, &channel.CTR2,
                                          &channel.CBR1, &channel.CSAR,
                                          &channel.CDAR, &channel.CLLR};
        unsigned index = 0U;
        for (unsigned n = 0U; n < 6U; ++n)
            if ((link & flags[n]) != 0U)
                reg(registers[n]) = words[index++];
        if ((reg(&channel.CTR2) & DMA_CTR2_TRIGPOL) == 0U)
            triggered = pending_hit = false;
        return;
    }
    reg(&channel.CCR) &= ~static_cast<uint32_t>(DMA_CCR_EN);
    reg(&channel.CSR) |= DMA_CSR_IDLEF;
    if (event_mode == DMA_CTR2_TCEM || event_mode == DMA_CTR2_TCEM_1)
        reg(&channel.CSR) |= DMA_CSR_TCF;
    if ((reg(&channel.CCR) & DMA_CCR_TCIE) != 0U)
        reg(&NVIC->ISPR[irq_word]) |= irq_bit;
}

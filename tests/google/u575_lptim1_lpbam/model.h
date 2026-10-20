/** \file model.h
 * \brief Independent SRAM4/LPTIM1 bus model for the real C receiver.
 * \author Nexus Team
 */
#ifndef NX_U575_LPDMA_COPY_MODEL_H
#define NX_U575_LPDMA_COPY_MODEL_H
#include "stm32u575_lpdma_copy_hw.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
struct LpdmaModel {
    std::unordered_map<uintptr_t, uint32_t> values;
    std::vector<std::pair<uintptr_t, uint32_t>> lptim_writes;
    unsigned reads{}, writes{}, dma_reads{}, dma_writes{}, resets{};
    bool hold_suspend{}, hold_reset{}, hold_clear{}, hold_irq{},
        hold_privilege{}, hold_write_ok{};
    bool triggered{}, pending_hit{}, link_pending{}, suspended{};
    uint32_t& reg(volatile const uint32_t* address) {
        return values[reinterpret_cast<uintptr_t>(address)];
    }
    void reset();
    void trigger();
    void lptim_event();
    void transfer(size_t bytes);
    void finish_block();
};
extern LpdmaModel model;
extern "C" void nx_u575_lptim1_dma_write(uintptr_t address, uint32_t value);
#endif

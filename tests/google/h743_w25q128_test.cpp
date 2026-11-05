/**
 * \file            h743_w25q128_test.cpp
 * \brief           Production flash protocol and portable owner at a HAL seam
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_w25q128_model.h"
#include "stm32h743_w25q128.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

QUADSPI_TypeDef g_test_qspi = {};
RCC_TypeDef g_test_rcc = {};
SCB_Type g_test_core = {};
MPU_Type g_test_mpu = {};
uint8_t g_test_aperture[16777216U];

namespace {
using testing::_;
using testing::NiceMock;
using testing::Return;

/** \brief Model the complete HAL boundary, not the production state machine.
 * \note Vendor HAL C is not linked here. A queued command completes at most
 * once per mocked IRQHandler call. Actual HAL/ARM links are separate evidence.
 */
struct Hardware {
    MOCK_METHOD(HAL_StatusTypeDef, Command,
                (QSPI_HandleTypeDef*, QSPI_CommandTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, Receive, (QSPI_HandleTypeDef*, uint8_t*));
    MOCK_METHOD(HAL_StatusTypeDef, Transmit, (QSPI_HandleTypeDef*, uint8_t*));
    MOCK_METHOD(void, Irq, (QSPI_HandleTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, MemoryMapped,
                (QSPI_HandleTypeDef*, QSPI_CommandTypeDef*,
                 QSPI_MemoryMappedTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, Abort, (QSPI_HandleTypeDef*));
    MOCK_METHOD(HAL_StatusTypeDef, Register,
                (QSPI_HandleTypeDef*, HAL_QSPI_CallbackIDTypeDef,
                 pQSPI_CallbackTypeDef));
    MOCK_METHOD(uint32_t, Read, (const volatile uint32_t*));
    MOCK_METHOD(void, Write, (volatile uint32_t*, uint32_t));

    enum class Pending { None, Command, Receive, Transmit, Abort };
    Pending pending = Pending::None;
    QSPI_CommandTypeDef command = {};
    std::vector<QSPI_CommandTypeDef> commands;
    std::array<uint8_t, 3> id = {0xef, 0x40, 0x18};
    uint8_t sr2 = 2U;
    uint8_t sr3 = 0U;
    uint8_t protection = 0U;
    bool wel = false;
    bool wip = false;
    bool ignore_wren = false;
    bool corrupt_program = false;
    bool short_status2 = false;
    bool refuse_address = false;
    bool inject_completion_te = false;
    bool completion_te_injected = false;
    uint32_t completion_te_opcode = 0x9fU;
    bool hold_wip = false;
    bool hold_abort = false;
    bool irq_idle = true;
    bool irq_pending = false;
    bool refuse_pending_clear = false;
    volatile uint32_t* refused_register = nullptr;
    unsigned busy_reads = 0U;
    size_t irq_calls = 0U;
    size_t program_payloads = 0U;
    struct Region {
        uint32_t rbar = 0U;
        uint32_t rasr = 0U;
    };
    std::array<Region, 16> regions = {};
    unsigned refuse_rasr_bank = 16U;
    uint32_t refuse_rnr_value = UINT32_MAX;
    bool enabled_region_base_changed = false;
    std::vector<unsigned> guard_events;
    enum class Boundary { Command, Receive, Transmit, DataComplete };
    bool track_boundaries = false;
    size_t fault_boundary = SIZE_MAX;
    bool boundary_fault_injected = false;
    size_t payloads_at_fault = 0U;
    std::vector<Boundary> boundaries;

    bool boundary(Boundary kind) {
        if (!track_boundaries) {
            return false;
        }
        const size_t ordinal = boundaries.size();
        boundaries.push_back(kind);
        if (ordinal != fault_boundary) {
            return false;
        }
        boundary_fault_injected = true;
        payloads_at_fault = program_payloads;
        return true;
    }

    static HAL_StatusTypeDef register_callback(QSPI_HandleTypeDef* h,
                                               HAL_QSPI_CallbackIDTypeDef id,
                                               pQSPI_CallbackTypeDef callback) {
        switch (id) {
            case HAL_QSPI_ERROR_CB_ID:
                h->ErrorCallback = callback;
                break;
            case HAL_QSPI_ABORT_CB_ID:
                h->AbortCpltCallback = callback;
                break;
            case HAL_QSPI_CMD_CPLT_CB_ID:
                h->CmdCpltCallback = callback;
                break;
            case HAL_QSPI_RX_CPLT_CB_ID:
                h->RxCpltCallback = callback;
                break;
            case HAL_QSPI_TX_CPLT_CB_ID:
                h->TxCpltCallback = callback;
                break;
            default:
                return HAL_ERROR;
        }
        return HAL_OK;
    }

    HAL_StatusTypeDef queue_command(QSPI_HandleTypeDef* h,
                                    QSPI_CommandTypeDef* c) {
        EXPECT_EQ(pending, Pending::None);
        EXPECT_EQ(c->InstructionMode, QSPI_INSTRUCTION_1_LINE);
        EXPECT_EQ(c->DdrMode, QSPI_DDR_MODE_DISABLE);
        EXPECT_EQ(c->AlternateByteMode, QSPI_ALTERNATE_BYTES_NONE);
        command = *c;
        commands.push_back(*c);
        g_test_qspi.CCR =
            c->Instruction | c->InstructionMode | c->AddressMode | c->DataMode |
            (c->AddressMode != QSPI_ADDRESS_NONE ? c->AddressSize : 0U) |
            (c->DummyCycles << QUADSPI_CCR_DCYC_Pos);
        if (!refuse_address || c->AddressMode == QSPI_ADDRESS_NONE) {
            g_test_qspi.AR = c->Address;
        }
        command.Address = g_test_qspi.AR;
        if (c->DataMode != QSPI_DATA_NONE) {
            EXPECT_EQ(c->DataMode, QSPI_DATA_1_LINE);
            EXPECT_GT(c->NbData, 0U);
            g_test_qspi.DLR = c->NbData - 1U;
            h->State = HAL_QSPI_STATE_READY;
        } else {
            g_test_qspi.SR &=
                ~static_cast<uint32_t>(QUADSPI_SR_TEF | QUADSPI_SR_TCF);
            pending = Pending::Command;
            h->State = HAL_QSPI_STATE_BUSY;
            g_test_qspi.CR |= QUADSPI_CR_TCIE | QUADSPI_CR_TEIE;
            g_test_qspi.SR |= QUADSPI_SR_BUSY | QUADSPI_SR_TCF;
            irq_pending = true;
        }
        return HAL_OK;
    }

    HAL_StatusTypeDef queue_data(QSPI_HandleTypeDef* h, uint8_t* data,
                                 bool receive) {
        EXPECT_EQ(pending, Pending::None);
        g_test_qspi.SR &=
            ~static_cast<uint32_t>(QUADSPI_SR_TEF | QUADSPI_SR_TCF);
        pending = receive ? Pending::Receive : Pending::Transmit;
        if (receive) {
            h->pRxBuffPtr = data;
            h->RxXferSize = h->RxXferCount = command.NbData;
            h->State = HAL_QSPI_STATE_BUSY_INDIRECT_RX;
        } else {
            h->pTxBuffPtr = data;
            h->TxXferSize = h->TxXferCount = command.NbData;
            h->State = HAL_QSPI_STATE_BUSY_INDIRECT_TX;
        }
        g_test_qspi.CR |= QUADSPI_CR_TCIE | QUADSPI_CR_TEIE | QUADSPI_CR_FTIE;
        g_test_qspi.CCR =
            (g_test_qspi.CCR & ~static_cast<uint32_t>(QUADSPI_CCR_FMODE)) |
            (receive ? static_cast<uint32_t>(QUADSPI_CCR_FMODE_0) : 0U);
        g_test_qspi.SR |= QUADSPI_SR_BUSY | QUADSPI_SR_FTF;
        irq_pending = true;
        return HAL_OK;
    }

    uint8_t status1() {
        if (wip && !hold_wip && busy_reads == 0U) {
            wip = false;
            wel = false;
        }
        const uint8_t value = protection | (wip ? 1U : 0U) | (wel ? 2U : 0U);
        if (wip && !hold_wip && busy_reads != 0U) {
            --busy_reads;
        }
        return value;
    }

    void pulse() {
        EXPECT_TRUE(wel);
        wip = true;
        busy_reads = 2U;
    }

    void receive(QSPI_HandleTypeDef* h) {
        ASSERT_NE(h->pRxBuffPtr, nullptr);
        if (short_status2 && command.Instruction == 0x35U) {
            /* Official TC handling can call RxCplt with a nonzero remainder
             * when the FIFO reports no more bytes. Do not invent fresh data.
             */
            h->RxXferCount = 1U;
            return;
        }
        if (command.Instruction == 0x03U) {
            ASSERT_LE(command.Address, sizeof(g_test_aperture));
            ASSERT_LE(command.NbData,
                      sizeof(g_test_aperture) - command.Address);
        }
        for (uint32_t i = 0U; i < command.NbData; ++i) {
            uint8_t value = 0xff;
            switch (command.Instruction) {
                case 0x9f:
                    value = id[i % id.size()];
                    break;
                case 0x05:
                    value = status1();
                    break;
                case 0x35:
                    value = sr2;
                    break;
                case 0x15:
                    value = sr3;
                    break;
                case 0x03:
                    EXPECT_FALSE(wip);
                    value = g_test_aperture[command.Address + i];
                    break;
                default:
                    ADD_FAILURE()
                        << "Unexpected read opcode " << command.Instruction;
            }
            h->pRxBuffPtr[i] = value;
        }
        h->pRxBuffPtr += command.NbData;
        h->RxXferCount = 0U;
    }

    void advance(QSPI_HandleTypeDef* h) {
        ++irq_calls;
        const Pending action = pending;
        if (action == Pending::None ||
            (action == Pending::Abort && hold_abort)) {
            return;
        }
        pending = Pending::None;
        pQSPI_CallbackTypeDef callback = nullptr;
        if (action == Pending::Receive) {
            receive(h);
            callback = h->RxCpltCallback;
        } else if (action == Pending::Transmit) {
            ASSERT_NE(h->pTxBuffPtr, nullptr);
            ASSERT_LE(command.Address, sizeof(g_test_aperture));
            ASSERT_LE(command.NbData,
                      sizeof(g_test_aperture) - command.Address);
            EXPECT_EQ(command.Instruction, 0x02U);
            EXPECT_LE((command.Address % 256U) + command.NbData, 256U);
            ++program_payloads;
            pulse();
            for (uint32_t i = 0U; i < command.NbData; ++i) {
                g_test_aperture[command.Address + i] &= h->pTxBuffPtr[i];
            }
            if (corrupt_program && command.NbData != 0U) {
                g_test_aperture[command.Address] ^= 1U;
            }
            h->pTxBuffPtr += command.NbData;
            h->TxXferCount = 0U;
            callback = h->TxCpltCallback;
        } else if (action == Pending::Command) {
            if (command.Instruction == 0x06U) {
                wel = !ignore_wren;
            } else if (command.Instruction == 0x04U) {
                wel = false;
            } else if (command.Instruction == 0x20U) {
                ASSERT_LE(command.Address, sizeof(g_test_aperture) - 4096U);
                EXPECT_EQ(command.Address % 4096U, 0U);
                pulse();
                std::memset(g_test_aperture + command.Address, 0xff, 4096U);
            } else {
                ADD_FAILURE()
                    << "Unexpected command-only opcode " << command.Instruction;
            }
            callback = h->CmdCpltCallback;
        } else {
            callback = h->AbortCpltCallback;
            g_test_qspi.CR &= ~static_cast<uint32_t>(QUADSPI_CR_ABORT);
            g_test_qspi.CCR &= ~static_cast<uint32_t>(QUADSPI_CCR_FMODE);
        }
        g_test_qspi.SR &=
            ~static_cast<uint32_t>(QUADSPI_SR_BUSY | QUADSPI_SR_FLEVEL |
                                   QUADSPI_SR_FTF | QUADSPI_SR_TCF);
        g_test_qspi.CR &= ~static_cast<uint32_t>(
            QUADSPI_CR_TCIE | QUADSPI_CR_TEIE | QUADSPI_CR_FTIE);
        const bool data_complete =
            action == Pending::Receive || action == Pending::Transmit;
        const bool boundary_te =
            data_complete && boundary(Boundary::DataComplete);
        if (boundary_te ||
            (inject_completion_te && !completion_te_injected && data_complete &&
             command.Instruction == completion_te_opcode)) {
            g_test_qspi.SR |= QUADSPI_SR_TEF;
            completion_te_injected = true;
        }
        h->State = HAL_QSPI_STATE_READY;
        irq_pending = true;
        ASSERT_NE(callback, nullptr);
        callback(h);
    }

    Hardware() {
        ON_CALL(*this, Command(_, _))
            .WillByDefault(
                [this](QSPI_HandleTypeDef* h, QSPI_CommandTypeDef* c) {
                    if (boundary(Boundary::Command)) {
                        return HAL_ERROR;
                    }
                    return queue_command(h, c);
                });
        ON_CALL(*this, Receive(_, _))
            .WillByDefault([this](QSPI_HandleTypeDef* h, uint8_t* p) {
                if (boundary(Boundary::Receive)) {
                    return HAL_ERROR;
                }
                return queue_data(h, p, true);
            });
        ON_CALL(*this, Transmit(_, _))
            .WillByDefault([this](QSPI_HandleTypeDef* h, uint8_t* p) {
                if (boundary(Boundary::Transmit)) {
                    return HAL_ERROR;
                }
                return queue_data(h, p, false);
            });
        ON_CALL(*this, Irq(_)).WillByDefault([this](QSPI_HandleTypeDef* h) {
            advance(h);
        });
        ON_CALL(*this, Register(_, _, _)).WillByDefault(register_callback);
        ON_CALL(*this, MemoryMapped(_, _, _))
            .WillByDefault([this](QSPI_HandleTypeDef* h, QSPI_CommandTypeDef* c,
                                  QSPI_MemoryMappedTypeDef*) {
                EXPECT_FALSE(wip);
                EXPECT_EQ(c->Instruction, 0x03U);
                EXPECT_EQ(c->AddressSize, QSPI_ADDRESS_24_BITS);
                EXPECT_EQ(c->DummyCycles, 0U);
                g_test_qspi.CCR = c->Instruction | c->InstructionMode |
                                  c->AddressMode | c->AddressSize |
                                  c->DataMode |
                                  static_cast<uint32_t>(QUADSPI_CCR_FMODE);
                h->State = HAL_QSPI_STATE_BUSY_MEM_MAPPED;
                return HAL_OK;
            });
        ON_CALL(*this, Abort(_)).WillByDefault([this](QSPI_HandleTypeDef* h) {
            guard_events.push_back(3U);
            if ((static_cast<uint32_t>(h->State) & 2U) == 0U) {
                return HAL_OK;
            }
            g_test_qspi.CR &= ~static_cast<uint32_t>(
                QUADSPI_CR_TCIE | QUADSPI_CR_TEIE | QUADSPI_CR_FTIE |
                QUADSPI_CR_SMIE | QUADSPI_CR_TOIE);
            if ((g_test_qspi.SR & QUADSPI_SR_BUSY) == 0U) {
                h->State = HAL_QSPI_STATE_READY;
                return HAL_OK;
            }
            pending = Pending::Abort;
            g_test_qspi.CR |= QUADSPI_CR_ABORT | QUADSPI_CR_TCIE;
            g_test_qspi.SR |= QUADSPI_SR_BUSY | QUADSPI_SR_TCF;
            irq_pending = true;
            h->State = HAL_QSPI_STATE_ABORT;
            return HAL_OK;
        });
        ON_CALL(*this, Read(_))
            .WillByDefault([this](const volatile uint32_t* p) {
                if (p == &g_test_mpu.TYPE) {
                    return uint32_t{16U << MPU_TYPE_DREGION_Pos};
                }
                if (p == &g_test_mpu.RBAR) {
                    return (regions[g_test_mpu.RNR % regions.size()].rbar &
                            static_cast<uint32_t>(MPU_RBAR_ADDR_Msk)) |
                           (g_test_mpu.RNR &
                            static_cast<uint32_t>(MPU_RBAR_REGION_Msk));
                }
                if (p == &g_test_mpu.RASR) {
                    return regions[g_test_mpu.RNR % regions.size()].rasr;
                }
                return *p;
            });
        ON_CALL(*this, Write(_, _))
            .WillByDefault([this](volatile uint32_t* p, uint32_t v) {
                if (p == refused_register) {
                    return;
                }
                if (p == &g_test_qspi.AR && refuse_address) {
                    return;
                }
                if (p == &g_test_mpu.RNR) {
                    if (v != refuse_rnr_value) {
                        g_test_mpu.RNR = v;
                    }
                } else if (p == &g_test_mpu.RBAR) {
                    if ((v & MPU_RBAR_VALID_Msk) != 0U) {
                        g_test_mpu.RNR = v & MPU_RBAR_REGION_Msk;
                    }
                    Region& region = regions[g_test_mpu.RNR % regions.size()];
                    const uint32_t base = v & MPU_RBAR_ADDR_Msk;
                    if ((region.rasr & MPU_RASR_ENABLE_Msk) != 0U &&
                        region.rbar != base) {
                        enabled_region_base_changed = true;
                    }
                    region.rbar = base;
                } else if (p == &g_test_mpu.RASR) {
                    const size_t bank = g_test_mpu.RNR % regions.size();
                    if (bank != refuse_rasr_bank) {
                        regions[bank].rasr = v;
                    }
                    if (bank == 13U) {
                        guard_events.push_back(
                            (v & MPU_RASR_ENABLE_Msk) != 0U ? 1U : 2U);
                    }
                } else if (p == &g_test_qspi.FCR) {
                    g_test_qspi.SR &= ~v;
                } else {
                    *p = v;
                }
            });
    }

    size_t count(uint32_t opcode) const {
        return static_cast<size_t>(std::count_if(
            commands.begin(), commands.end(),
            [opcode](const auto& c) { return c.Instruction == opcode; }));
    }
};
Hardware* hardware = nullptr;

class H743W25Q128 : public testing::Test {
  protected:
    NiceMock<Hardware> hw;
    nx_h743_w25q128_state_t state = {};
    nx_mapped_storage_backend_t backend = {};
    nx_mapped_storage_owner_t owner = {};
    nx_mapped_storage_slot_t slots[2] = {};
    const nx_mapped_storage_port_t port{&nx_mapped_storage_owner_ops, &owner};
    nx_mapped_storage_request_t request = {};
    nx_mapped_storage_ticket_t ticket = {};
    std::array<uint8_t, 600> input = {};
    nx_time_us_t now = 1U;
    nx_h743_w25q128_mpu_config_t mpu = {12U, 13U};

    static nx_time_us_t read_clock(void* context) {
        return *static_cast<nx_time_us_t*>(context);
    }

    void reset_context(Hardware& model) {
        hardware = &model;
        state = {};
        backend = {};
        owner = {};
        slots[0] = {};
        slots[1] = {};
        request = {};
        ticket = {};
        now = 1U;
        mpu = {12U, 13U};
        std::memset(&g_test_qspi, 0, sizeof(g_test_qspi));
        std::memset(&g_test_rcc, 0, sizeof(g_test_rcc));
        g_test_core.CCR = 0U;
        g_test_mpu.CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_HFNMIENA_Msk;
        g_test_mpu.RNR = 3U;
        model.regions[12].rbar = 0x60000000U;
        model.regions[13].rbar = 0x70000000U;
        std::memset(g_test_aperture, 0xff, sizeof(g_test_aperture));
        g_test_rcc.AHB3ENR = RCC_AHB3ENR_QSPIEN;
        input.fill(0xa5);
        nx_request_initialize(&request.base);
    }

    void SetUp() override {
        reset_context(hw);
    }

    void TearDown() override {
        hardware = nullptr;
    }

    nx_result_t prepare() {
        return nx_h743_w25q128_prepare(&state, {read_clock, &now}, 64000000U,
                                       16000000U, mpu);
    }

    void step() {
        now += 100U;
        nx_h743_w25q128_service(&state);
        if (owner.active != nullptr) {
            nx_mapped_storage_service(&port);
        }
    }

    bool boot() {
        const nx_result_t admitted = prepare();
        EXPECT_EQ(admitted, NX_SUCCESS);
        if (admitted != NX_SUCCESS) {
            return false;
        }
        for (unsigned i = 0U;
             i < 600U && nx_h743_w25q128_status(&state) == NX_ERROR_BUSY; ++i) {
            step();
        }
        const nx_result_t discovered = nx_h743_w25q128_status(&state);
        EXPECT_EQ(discovered, NX_SUCCESS);
        return discovered == NX_SUCCESS;
    }

    bool bind() {
        const nx_result_t exported = nx_h743_w25q128_backend(&state, &backend);
        EXPECT_EQ(exported, NX_SUCCESS);
        if (exported != NX_SUCCESS) {
            return false;
        }
        const nx_result_t bound = nx_mapped_storage_owner_init(
            &owner, slots, 2U, backend, {read_clock, &now});
        EXPECT_EQ(bound, NX_SUCCESS);
        return bound == NX_SUCCESS;
    }

    void program(size_t offset, size_t length,
                 nx_time_us_t deadline = NX_DEADLINE_NEVER) {
        ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM,
                                            offset, input.data(), length,
                                            deadline),
                  NX_SUCCESS);
        ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
                  NX_SUCCESS);
    }

    void settle() {
        for (unsigned i = 0U; i < 10000U && nx_request_state(&request.base) !=
                                                NX_REQUEST_SETTLED;
             ++i) {
            step();
        }
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    }

    void expect_result(nx_result_t expected, size_t count) {
        nx_result_t result = NX_ERROR_STATE;
        size_t completed = SIZE_MAX;
        ASSERT_EQ(nx_request_result(&request.base, &result, &completed),
                  NX_SUCCESS);
        EXPECT_EQ(result, expected);
        EXPECT_EQ(completed, count);
        EXPECT_EQ(state.input, nullptr);
        EXPECT_EQ(state.hal.pTxBuffPtr, nullptr);
        EXPECT_EQ(state.hal.pRxBuffPtr, nullptr);
        EXPECT_EQ(g_test_qspi.SR & (QUADSPI_SR_BUSY | QUADSPI_SR_FLEVEL), 0U);
    }
};

TEST_F(H743W25Q128, ColdDiscoveryWaitsPowerGuardAndExportsOnlyCpuGeometry) {
    backend.context_size = 123U;
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_EQ(nx_h743_w25q128_status(&state), NX_ERROR_BUSY);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_EQ(backend.context_size, 123U);
    now += 4999U;
    nx_h743_w25q128_service(&state);
    EXPECT_TRUE(hw.commands.empty());
    for (unsigned i = 0U;
         i < 100U && nx_h743_w25q128_status(&state) == NX_ERROR_BUSY; ++i) {
        step();
    }
    ASSERT_EQ(nx_h743_w25q128_status(&state), NX_SUCCESS);
    ASSERT_EQ(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_EQ(backend.geometry.mapped_base, g_test_aperture);
    EXPECT_EQ(backend.geometry.size, 16777216U);
    EXPECT_EQ(backend.geometry.program_unit, 1U);
    EXPECT_EQ(backend.geometry.erase_unit, 4096U);
    EXPECT_EQ(backend.geometry.readers, NX_MAPPED_STORAGE_CPU_READ);
    EXPECT_EQ(hw.count(0x9f), 1U);
    EXPECT_EQ(hw.count(0x01) + hw.count(0x31) + hw.count(0x11) +
                  hw.count(0x66) + hw.count(0x99),
              0U);
}

TEST_F(H743W25Q128, CacheOrUnleasedControllerRejectsWithoutAcquisition) {
    g_test_core.CCR = SCB_CCR_DC_Msk;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    g_test_core.CCR = SCB_CCR_IC_Msk;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    g_test_core.CCR = 0U;
    g_test_rcc.AHB3ENR = 0U;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    g_test_rcc.AHB3ENR = RCC_AHB3ENR_QSPIEN;
    hw.irq_idle = false;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_TRUE(hw.commands.empty());
}

TEST_F(H743W25Q128, UnsupportedIdentityRetainsCleanupAndNeverExportsBackend) {
    hw.id = {0xef, 0x70, 0x18};
    ASSERT_EQ(prepare(), NX_SUCCESS);
    for (unsigned i = 0U; i < 600U; ++i) {
        step();
    }
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    backend.context_size = 321U;
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_EQ(backend.context_size, 321U);
    EXPECT_EQ(hw.count(0x06), 0U);
}

TEST_F(H743W25Q128, ReadLoanExcludesMutationAndFailedUnmapKeepsLoan) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    nx_mapped_storage_loan_t loan = {};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 4U,
                                        16U, &loan),
              NX_SUCCESS);
    EXPECT_EQ(loan.data, g_test_aperture + 4U);
    ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_PROGRAM, 0U,
                                        input.data(), 16U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_submit(&port, &request, &ticket),
              NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_ERROR_BUSY);
    EXPECT_TRUE(state.owned);
    EXPECT_CALL(hw, Abort(_)).WillOnce(Return(HAL_ERROR));
    EXPECT_NE(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    EXPECT_EQ(owner.loans, 1U);
    EXPECT_TRUE(slots[loan.slot].live);
    testing::Mock::VerifyAndClearExpectations(&hw);
    nx_result_t released = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 100U && released != NX_SUCCESS; ++i) {
        step();
        released = nx_mapped_storage_release(&port, &loan);
    }
    EXPECT_EQ(released, NX_SUCCESS);
    EXPECT_EQ(owner.loans, 0U);
}

TEST_F(H743W25Q128, ProgramSplitsPagesAndConfirmsReadbackBeforeSettlement) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    program(250U, input.size());
    settle();
    expect_result(NX_SUCCESS, input.size());
    EXPECT_TRUE(std::equal(input.begin(), input.end(), g_test_aperture + 250U));
    std::vector<uint32_t> lengths;
    for (size_t i = 0U; i < hw.commands.size(); ++i) {
        const auto& c = hw.commands[i];
        if (c.Instruction == 0x02U) {
            lengths.push_back(c.NbData);
            EXPECT_EQ(c.AddressSize, QSPI_ADDRESS_24_BITS);
            size_t verified = 0U;
            for (size_t j = i + 1U;
                 j < hw.commands.size() && hw.commands[j].Instruction != 0x02U;
                 ++j) {
                const auto& read = hw.commands[j];
                if (read.Instruction == 0x03U && read.Address >= c.Address &&
                    read.Address - c.Address < c.NbData) {
                    verified += read.NbData;
                }
            }
            EXPECT_GE(verified, c.NbData);
        }
    }
    EXPECT_THAT(lengths, testing::ElementsAre(6U, 256U, 256U, 82U));
    EXPECT_EQ(hw.count(0x06), lengths.size());
    EXPECT_GT(hw.count(0x03), 0U);
}

TEST_F(H743W25Q128, NonErasedProgramTargetFailsBeforeAnyWriteEnable) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    g_test_aperture[0] = 0U;
    program(0U, 16U);
    settle();
    expect_result(NX_ERROR_IO, 0U);
    EXPECT_EQ(hw.count(0x06), 0U);
    EXPECT_EQ(hw.count(0x02), 0U);
    EXPECT_EQ(g_test_aperture[0], 0U);
}

TEST_F(H743W25Q128, SectorEraseRetainsRequestUntilFlashBusyClears) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    std::memset(g_test_aperture + 4096U, 0U, 4096U);
    hw.hold_wip = true;
    ASSERT_EQ(nx_mapped_storage_prepare(&request, NX_MAPPED_STORAGE_ERASE,
                                        4096U, nullptr, 4096U,
                                        NX_DEADLINE_NEVER),
              NX_SUCCESS);
    ASSERT_EQ(nx_mapped_storage_submit(&port, &request, &ticket), NX_SUCCESS);
    for (unsigned i = 0U; i < 1000U && !hw.wip; ++i) {
        step();
    }
    ASSERT_TRUE(hw.wip);
    EXPECT_EQ(hw.count(0x20), 1U);
    for (unsigned i = 0U; i < 20U; ++i) {
        step();
    }
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.active, &request);
    hw.hold_wip = false;
    settle();
    expect_result(NX_SUCCESS, 4096U);
    EXPECT_TRUE(std::all_of(g_test_aperture + 4096U, g_test_aperture + 8192U,
                            [](uint8_t b) { return b == 0xffU; }));
}

TEST_F(H743W25Q128, CancelAfterIssuedPageRetainsInputAndSkipsFuturePages) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.hold_wip = true;
    program(0U, input.size());
    for (unsigned i = 0U; i < 1500U && !hw.wip; ++i) {
        step();
    }
    ASSERT_TRUE(hw.wip);
    ASSERT_EQ(nx_mapped_storage_cancel(&port, ticket), NX_SUCCESS);
    for (unsigned i = 0U; i < 20U; ++i) {
        step();
    }
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.active, &request);
    EXPECT_EQ(state.input, input.data());
    EXPECT_EQ(hw.count(0x02), 1U);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_ERROR_BUSY);
    hw.hold_wip = false;
    settle();
    nx_result_t result = NX_SUCCESS;
    size_t completed = SIZE_MAX;
    ASSERT_EQ(nx_request_result(&request.base, &result, &completed),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_CANCELLED);
    EXPECT_LE(completed, 256U);
    EXPECT_EQ(hw.count(0x02), 1U);
    EXPECT_EQ(state.input, nullptr);
}
TEST_F(H743W25Q128, ProtectedArrayRejectsWithoutChangingStatusRegisters) {
    hw.protection = 4U;
    ASSERT_EQ(prepare(), NX_SUCCESS);
    for (unsigned i = 0U; i < 600U; ++i) {
        step();
    }
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_EQ(hw.count(0x01) + hw.count(0x31) + hw.count(0x11) + hw.count(0x98),
              0U);
    EXPECT_EQ(hw.protection, 4U);
}

TEST_F(H743W25Q128, FailedWriteEnableNeverIssuesProgram) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.ignore_wren = true;
    program(0U, 1U);
    settle();
    expect_result(NX_ERROR_IO, 0U);
    EXPECT_EQ(hw.count(0x06), 1U);
    EXPECT_EQ(hw.count(0x02), 0U);
    EXPECT_EQ(g_test_aperture[0], 0xffU);
}

TEST_F(H743W25Q128, CorruptedProgramReadbackCannotPublishSuccess) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.corrupt_program = true;
    program(0U, 16U);
    settle();
    expect_result(NX_ERROR_IO, 0U);
    EXPECT_EQ(hw.count(0x02), 1U);
    EXPECT_NE(g_test_aperture[0], input[0]);
}

TEST_F(H743W25Q128, RequestDeadlineDoesNotReleaseAnIssuedFlashPulse) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.hold_wip = true;
    program(0U, 1U, now + 2500U);
    for (unsigned i = 0U; i < 100U && !hw.wip; ++i) {
        step();
    }
    ASSERT_TRUE(hw.wip);
    now = request.base.deadline + 1U;
    step();
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.active, &request);
    EXPECT_EQ(state.input, input.data());
    EXPECT_EQ(hw.count(0x02), 1U);
    hw.hold_wip = false;
    settle();
    nx_result_t result = NX_SUCCESS;
    size_t completed = SIZE_MAX;
    ASSERT_EQ(nx_request_result(&request.base, &result, &completed),
              NX_SUCCESS);
    EXPECT_EQ(result, NX_ERROR_TIMEOUT);
    EXPECT_LE(completed, 1U);
    EXPECT_EQ(state.input, nullptr);
}

TEST_F(H743W25Q128, FailedAbortKeepsReadLoanAndControllerUntilRetry) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    nx_mapped_storage_loan_t loan = {};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        16U, &loan),
              NX_SUCCESS);
    hw.hold_abort = true;
    g_test_qspi.SR |= QUADSPI_SR_BUSY;
    EXPECT_NE(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    for (unsigned i = 0U; i < 20U; ++i) {
        step();
        EXPECT_NE(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    }
    EXPECT_EQ(owner.loans, 1U);
    EXPECT_TRUE(slots[loan.slot].live);
    EXPECT_TRUE(state.owned);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_ERROR_BUSY);
    hw.hold_abort = false;
    nx_result_t released = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 100U && released != NX_SUCCESS; ++i) {
        step();
        released = nx_mapped_storage_release(&port, &loan);
    }
    EXPECT_EQ(released, NX_SUCCESS);
    EXPECT_EQ(nx_mapped_storage_stop(&port), NX_SUCCESS);
    nx_result_t stopped = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 100U && stopped != NX_SUCCESS; ++i) {
        step();
        stopped = nx_h743_w25q128_stop(&state);
    }
    EXPECT_EQ(stopped, NX_SUCCESS);
    EXPECT_FALSE(state.owned);
}

TEST_F(H743W25Q128, PendingClearRefusalPreventsRequestBorrowRelease) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.refuse_pending_clear = true;
    program(0U, 1U);
    for (unsigned i = 0U; i < 500U; ++i) {
        step();
    }
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.active, &request);
    EXPECT_EQ(state.input, input.data());
    EXPECT_TRUE(state.owned);
    hw.refuse_pending_clear = false;
    settle();
    EXPECT_EQ(state.input, nullptr);
}

TEST_F(H743W25Q128, RefusedColdConfigurationRetainsCleanupResponsibility) {
    hw.refused_register = &g_test_qspi.DCR;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_TRUE(hw.commands.empty());
}

TEST_F(H743W25Q128, HalReceiveAdmissionFailureNeverExportsDiscoveredBackend) {
    EXPECT_CALL(hw, Receive(_, _)).WillOnce(Return(HAL_ERROR));
    ASSERT_EQ(prepare(), NX_SUCCESS);
    for (unsigned i = 0U; i < 600U; ++i) {
        step();
    }
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
}

TEST_F(H743W25Q128, FailedTransmitAfterWelDrainsThroughHealthyStatusAndWrdi) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    EXPECT_CALL(hw, Transmit(_, _)).WillOnce(Return(HAL_ERROR));
    program(0U, 16U);
    settle();
    expect_result(NX_ERROR_IO, 0U);
    EXPECT_EQ(hw.count(0x06), 1U);
    EXPECT_GT(hw.count(0x04), 0U);
    EXPECT_FALSE(hw.wel);
    EXPECT_FALSE(hw.wip);
    EXPECT_EQ(hw.program_payloads, 0U);
    EXPECT_TRUE(std::all_of(g_test_aperture, g_test_aperture + 16U,
                            [](uint8_t b) { return b == 0xffU; }));
}

TEST_F(H743W25Q128, AcceptedColdStartFailureDrainsAfterClockReturns) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    g_test_rcc.AHB3ENR = 0U;
    program(0U, 16U);
    ASSERT_EQ(owner.active, &request);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(hw.count(0x06), 0U);
    EXPECT_EQ(hw.count(0x02), 0U);
    g_test_rcc.AHB3ENR = RCC_AHB3ENR_QSPIEN;
    for (unsigned i = 0U;
         i < 1000U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
         ++i) {
        step();
    }
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_result_t result = NX_SUCCESS;
    size_t transferred = SIZE_MAX;
    ASSERT_EQ(nx_request_result(&request.base, &result, &transferred),
              NX_SUCCESS);
    EXPECT_NE(result, NX_SUCCESS);
    EXPECT_EQ(transferred, 0U);
    EXPECT_EQ(owner.active, nullptr);
    EXPECT_EQ(state.input, nullptr);
    EXPECT_EQ(state.hal.pTxBuffPtr, nullptr);
    EXPECT_EQ(state.hal.pRxBuffPtr, nullptr);
    EXPECT_EQ(hw.count(0x06), 0U);
    EXPECT_EQ(hw.count(0x02), 0U);
    EXPECT_FALSE(hw.wel);
    EXPECT_FALSE(hw.wip);
}

TEST_F(H743W25Q128, SuspendedFlashIsRejectedBeforeBackendExport) {
    hw.sr2 = 0x82U;
    ASSERT_EQ(prepare(), NX_SUCCESS);
    for (unsigned i = 0U; i < 600U; ++i) {
        step();
    }
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_EQ(hw.count(0x7a) + hw.count(0x66) + hw.count(0x99), 0U);
}

TEST_F(H743W25Q128, ShortStatusRxCannotReuseThePreviousUnprotectedByte) {
    hw.sr2 = 0x42U;
    hw.short_status2 = true;
    ASSERT_EQ(prepare(), NX_SUCCESS);
    for (unsigned i = 0U; i < 600U; ++i) {
        step();
    }
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
}

TEST_F(H743W25Q128, TransferErrorCannotHideBehindNormalCompletionCallback) {
    hw.inject_completion_te = true;
    ASSERT_EQ(prepare(), NX_SUCCESS);
    for (unsigned i = 0U; i < 600U; ++i) {
        step();
    }
    EXPECT_TRUE(hw.completion_te_injected);
    EXPECT_NE(nx_h743_w25q128_status(&state), NX_SUCCESS);
    EXPECT_NE(nx_h743_w25q128_backend(&state, &backend), NX_SUCCESS);
    EXPECT_EQ(hw.count(0x05), 0U);
}

TEST_F(H743W25Q128, TransferErrorAfterIssuedPageDrainsBeforeReleasingInput) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.inject_completion_te = true;
    hw.completion_te_opcode = 0x02U;
    hw.hold_wip = true;
    program(0U, input.size());
    for (unsigned i = 0U; i < 1500U && !hw.completion_te_injected; ++i) {
        step();
    }
    ASSERT_TRUE(hw.completion_te_injected);
    ASSERT_TRUE(hw.wip);
    for (unsigned i = 0U; i < 20U; ++i) {
        step();
    }
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(owner.active, &request);
    EXPECT_EQ(state.input, input.data());
    EXPECT_EQ(hw.program_payloads, 1U);
    hw.hold_wip = false;
    for (unsigned i = 0U;
         i < 10000U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
         ++i) {
        step();
    }
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    expect_result(NX_ERROR_IO, 0U);
    EXPECT_EQ(hw.program_payloads, 1U);
    EXPECT_FALSE(hw.wip);
    EXPECT_FALSE(hw.wel);
}

TEST_F(H743W25Q128, RefusedAddressWriteNeverMutatesTheWrongFlashPage) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.refuse_address = true;
    program(4096U, 16U);
    settle();
    expect_result(NX_ERROR_IO, 0U);
    EXPECT_EQ(hw.count(0x02), 0U);
    EXPECT_TRUE(std::all_of(g_test_aperture, g_test_aperture + 16U,
                            [](uint8_t b) { return b == 0xffU; }));
    EXPECT_TRUE(std::all_of(g_test_aperture + 4096U, g_test_aperture + 4112U,
                            [](uint8_t b) { return b == 0xffU; }));
}

TEST_F(H743W25Q128, DisabledMpuOrMissingFaultCoverageRejectsColdAdmission) {
    g_test_mpu.CTRL = 0U;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    g_test_mpu.CTRL = MPU_CTRL_ENABLE_Msk;
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(g_test_qspi.CR, 0U);
    EXPECT_TRUE(hw.commands.empty());
}

TEST_F(H743W25Q128, ReversedOrOutOfRangeMpuRegionClaimsRejectWithoutEffects) {
    mpu = {13U, 12U};
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    mpu = {12U, 16U};
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(g_test_qspi.CR, 0U);
    EXPECT_TRUE(hw.commands.empty());
}

TEST_F(H743W25Q128, HigherPriorityReadableOverrideRejectsTheDenyGuard) {
    hw.regions[14].rbar = 0x90000000U;
    hw.regions[14].rasr = MPU_RASR_ENABLE_Msk | MPU_RASR_XN_Msk |
                          (uint32_t{3} << MPU_RASR_AP_Pos) |
                          (uint32_t{27} << MPU_RASR_SIZE_Pos);
    EXPECT_NE(prepare(), NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(hw.regions[12].rasr, 0U);
    EXPECT_EQ(hw.regions[13].rasr, 0U);
    EXPECT_TRUE(hw.commands.empty());
}

TEST_F(H743W25Q128, MpuDenyCoversWholeApertureAndMapGrantsOnlyUncachedRead) {
    ASSERT_EQ(prepare(), NX_SUCCESS);
    EXPECT_EQ(hw.regions[12].rbar & MPU_RBAR_ADDR_Msk, 0x90000000U);
    EXPECT_NE(hw.regions[12].rasr & MPU_RASR_ENABLE_Msk, 0U);
    EXPECT_NE(hw.regions[12].rasr & MPU_RASR_XN_Msk, 0U);
    EXPECT_EQ(hw.regions[12].rasr & MPU_RASR_AP_Msk, 0U);
    EXPECT_EQ((hw.regions[12].rasr & MPU_RASR_SIZE_Msk) >> MPU_RASR_SIZE_Pos,
              27U);
    EXPECT_EQ(hw.regions[13].rasr & MPU_RASR_ENABLE_Msk, 0U);
    for (unsigned i = 0U;
         i < 600U && nx_h743_w25q128_status(&state) == NX_ERROR_BUSY; ++i) {
        step();
    }
    ASSERT_EQ(nx_h743_w25q128_status(&state), NX_SUCCESS);
    ASSERT_TRUE(bind());
    nx_mapped_storage_loan_t loan = {};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        16U, &loan),
              NX_SUCCESS);
    const uint32_t readable = hw.regions[13].rasr;
    EXPECT_EQ(hw.regions[13].rbar & MPU_RBAR_ADDR_Msk, 0x90000000U);
    EXPECT_NE(readable & MPU_RASR_ENABLE_Msk, 0U);
    EXPECT_NE(readable & MPU_RASR_XN_Msk, 0U);
    EXPECT_EQ((readable & MPU_RASR_AP_Msk) >> MPU_RASR_AP_Pos, 6U);
    EXPECT_EQ((readable & MPU_RASR_SIZE_Msk) >> MPU_RASR_SIZE_Pos, 23U);
    EXPECT_EQ((readable & MPU_RASR_TEX_Msk) >> MPU_RASR_TEX_Pos, 1U);
    EXPECT_EQ(readable & (MPU_RASR_C_Msk | MPU_RASR_B_Msk), 0U);
    hw.guard_events.clear();
    nx_result_t released = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 100U && released != NX_SUCCESS; ++i) {
        step();
        released = nx_mapped_storage_release(&port, &loan);
    }
    EXPECT_EQ(released, NX_SUCCESS);
    EXPECT_EQ(hw.regions[13].rasr & MPU_RASR_ENABLE_Msk, 0U);
    const auto abort =
        std::find(hw.guard_events.begin(), hw.guard_events.end(), 3U);
    ASSERT_NE(abort, hw.guard_events.end());
    EXPECT_NE(std::find(hw.guard_events.begin(), abort, 2U), abort);
    EXPECT_EQ(g_test_mpu.CTRL, MPU_CTRL_ENABLE_Msk | MPU_CTRL_HFNMIENA_Msk);
}

TEST_F(H743W25Q128, RefusedReadRegionWithdrawalRetainsLoanAndClosesAdmission) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    nx_mapped_storage_loan_t loan = {};
    ASSERT_EQ(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        16U, &loan),
              NX_SUCCESS);
    hw.refuse_rasr_bank = 13U;
    EXPECT_CALL(hw, Abort(_)).Times(0);
    EXPECT_NE(nx_mapped_storage_release(&port, &loan), NX_SUCCESS);
    EXPECT_EQ(owner.loans, 1U);
    EXPECT_TRUE(slots[loan.slot].live);
    EXPECT_TRUE(state.owned);
    nx_mapped_storage_loan_t rejected = {};
    const nx_result_t admission = nx_mapped_storage_acquire(
        &port, NX_MAPPED_STORAGE_CPU_READ, 32U, 16U, &rejected);
    EXPECT_NE(admission, NX_SUCCESS);
    if (admission == NX_SUCCESS) {
        EXPECT_EQ(nx_mapped_storage_release(&port, &rejected), NX_SUCCESS);
    }
    testing::Mock::VerifyAndClearExpectations(&hw);
    hw.refuse_rasr_bank = 16U;
    nx_result_t released = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 100U && released != NX_SUCCESS; ++i) {
        step();
        released = nx_mapped_storage_release(&port, &loan);
    }
    EXPECT_EQ(released, NX_SUCCESS);
}

TEST_F(H743W25Q128,
       StopRestoresBothRegionBanksAndRetainsFailedSelectorRestore) {
    ASSERT_TRUE(boot());
    hw.refuse_rnr_value = 3U;
    EXPECT_NE(nx_h743_w25q128_stop(&state), NX_SUCCESS);
    EXPECT_TRUE(state.owned);
    hw.refuse_rnr_value = UINT32_MAX;
    nx_result_t stopped = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < 100U && stopped != NX_SUCCESS; ++i) {
        step();
        stopped = nx_h743_w25q128_stop(&state);
    }
    EXPECT_EQ(stopped, NX_SUCCESS);
    EXPECT_FALSE(state.owned);
    EXPECT_EQ(g_test_mpu.RNR, 3U);
    EXPECT_EQ(hw.regions[12].rbar, 0x60000000U);
    EXPECT_EQ(hw.regions[12].rasr, 0U);
    EXPECT_EQ(hw.regions[13].rbar, 0x70000000U);
    EXPECT_EQ(hw.regions[13].rasr, 0U);
}

TEST_F(H743W25Q128, RefusedReadRegionGrantCannotCreateAMapLoan) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.refuse_rasr_bank = 13U;
    nx_mapped_storage_loan_t loan = {};
    EXPECT_NE(nx_mapped_storage_acquire(&port, NX_MAPPED_STORAGE_CPU_READ, 0U,
                                        16U, &loan),
              NX_SUCCESS);
    EXPECT_EQ(owner.loans, 0U);
    EXPECT_NE(hw.regions[12].rasr & MPU_RASR_ENABLE_Msk, 0U);
    EXPECT_EQ(hw.regions[13].rasr & MPU_RASR_ENABLE_Msk, 0U);
}

TEST_F(H743W25Q128, StopDisablesRegionBeforeChangingItsBase) {
    ASSERT_TRUE(boot());
    ASSERT_NE(hw.regions[12].rasr & MPU_RASR_ENABLE_Msk, 0U);
    ASSERT_FALSE(hw.enabled_region_base_changed);
    ASSERT_EQ(nx_h743_w25q128_stop(&state), NX_SUCCESS);
    EXPECT_FALSE(hw.enabled_region_base_changed);
    EXPECT_EQ(hw.regions[12].rbar, 0x60000000U);
    EXPECT_EQ(hw.regions[12].rasr, 0U);
    EXPECT_EQ(hw.regions[13].rbar, 0x70000000U);
    EXPECT_EQ(hw.regions[13].rasr, 0U);
}

TEST_F(H743W25Q128, ObservedCrossPageBoundariesDrainOneShotTransportFaults) {
    ASSERT_TRUE(boot());
    ASSERT_TRUE(bind());
    hw.track_boundaries = true;
    program(250U, 32U);
    settle();
    expect_result(NX_SUCCESS, 32U);
    const auto observed = hw.boundaries;
    ASSERT_FALSE(observed.empty());
    ASSERT_LT(observed.size(), 128U);
    ASSERT_EQ(hw.program_payloads, 2U);
    RecordProperty("fault_scenarios", static_cast<int>(observed.size()));
    size_t complete_faults = 0U;
    for (size_t ordinal = 0U; ordinal < observed.size(); ++ordinal) {
        SCOPED_TRACE(testing::Message()
                     << "observed boundary " << ordinal << " kind "
                     << static_cast<unsigned>(observed[ordinal]));
        NiceMock<Hardware> model;
        reset_context(model);
        ASSERT_TRUE(boot());
        ASSERT_TRUE(bind());
        model.track_boundaries = true;
        model.fault_boundary = ordinal;
        program(250U, 32U);
        for (unsigned i = 0U;
             i < 2500U && nx_request_state(&request.base) != NX_REQUEST_SETTLED;
             ++i) {
            step();
        }
        ASSERT_TRUE(model.boundary_fault_injected);
        ASSERT_EQ(model.boundaries[ordinal], observed[ordinal]);
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        /* Only the first six-byte page can be verified before these faults.
         * The last observed boundary precedes normal final verification.
         */
        EXPECT_TRUE(request.base.transferred == 0U ||
                    request.base.transferred == 6U);
        expect_result(NX_ERROR_IO, request.base.transferred);
        EXPECT_FALSE(model.wel);
        EXPECT_FALSE(model.wip);
        EXPECT_EQ(model.program_payloads, model.payloads_at_fault);
        EXPECT_EQ(owner.active, nullptr);
        if (observed[ordinal] == Hardware::Boundary::DataComplete) {
            ++complete_faults;
        }
        hardware = &hw;
    }
    RecordProperty("completion_te_scenarios",
                   static_cast<int>(complete_faults));
}
} /* namespace */

extern "C" {
uint32_t nx_h743_w25q128_model_read(const volatile uint32_t* p) {
    return hardware->Read(p);
}
void nx_h743_w25q128_model_write(volatile uint32_t* p, uint32_t value) {
    hardware->Write(p, value);
}
bool nx_h743_w25q128_model_irq_idle(void) {
    return hardware->irq_idle;
}
bool nx_h743_w25q128_model_irq_pending(void) {
    return hardware->irq_pending;
}
void nx_h743_w25q128_model_clear_pending(void) {
    if (!hardware->refuse_pending_clear) {
        hardware->irq_pending = false;
    }
}
HAL_StatusTypeDef HAL_QSPI_RegisterCallback(QSPI_HandleTypeDef* h,
                                            HAL_QSPI_CallbackIDTypeDef id,
                                            pQSPI_CallbackTypeDef callback) {
    return hardware->Register(h, id, callback);
}
void HAL_QSPI_SetTimeout(QSPI_HandleTypeDef* h, uint32_t timeout) {
    h->Timeout = timeout;
}
HAL_StatusTypeDef HAL_QSPI_Command_IT(QSPI_HandleTypeDef* h,
                                      QSPI_CommandTypeDef* c) {
    return hardware->Command(h, c);
}
HAL_StatusTypeDef HAL_QSPI_Receive_IT(QSPI_HandleTypeDef* h, uint8_t* p) {
    return hardware->Receive(h, p);
}
HAL_StatusTypeDef HAL_QSPI_Transmit_IT(QSPI_HandleTypeDef* h, uint8_t* p) {
    return hardware->Transmit(h, p);
}
void HAL_QSPI_IRQHandler(QSPI_HandleTypeDef* h) {
    hardware->Irq(h);
}
HAL_StatusTypeDef HAL_QSPI_MemoryMapped(QSPI_HandleTypeDef* h,
                                        QSPI_CommandTypeDef* c,
                                        QSPI_MemoryMappedTypeDef* cfg) {
    return hardware->MemoryMapped(h, c, cfg);
}
HAL_StatusTypeDef HAL_QSPI_Abort_IT(QSPI_HandleTypeDef* h) {
    return hardware->Abort(h);
}
}

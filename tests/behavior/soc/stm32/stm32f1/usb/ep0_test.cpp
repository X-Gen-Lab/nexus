/**
 * \file            ep0_test.cpp
 *
 * \brief           STM32F1 EP0 write semantics and control packet ownership.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "link.h"
#include "pma_device.h"
#include <array>
#if defined(NX_TEST_PMA_ST_DEVICE)
#include "st_bulk.h"
#endif

#include <gtest/gtest.h>

namespace {
constexpr uint16_t ctr_mask = USB_EP_CTR_RX | USB_EP_CTR_TX;
constexpr uint16_t lp_sources =
    USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
constexpr uint16_t toggle_mask =
    USB_EPRX_STAT | USB_EPTX_STAT | USB_EP_DTOG_RX | USB_EP_DTOG_TX;
constexpr uint16_t direct_mask =
    USB_EP_TYPE_MASK | USB_EP_KIND | USB_EPADDR_FIELD;
constexpr uint16_t both_nak = USB_EP_RX_NAK | USB_EP_TX_NAK;
constexpr uint16_t setup_flags = USB_EP_CTR_RX | USB_EP_SETUP;
uint16_t inject_next_write;

uint16_t last_word;
nx_time_us_t model_now;
volatile uint16_t* model_endpoint;
volatile uint16_t* model_status;
#if defined(NX_STM32F1_USB_PMA_EP1)
volatile uint16_t* model_ep1;
#endif
constexpr uint16_t istr_w0c = USB_ISTR_ESOF | USB_ISTR_SOF | USB_ISTR_RESET |
                              USB_ISTR_SUSP | USB_ISTR_WKUP | USB_ISTR_ERR |
                              USB_ISTR_PMAOVR;
} /* namespace */

extern "C" nx_time_us_t nx_time_now_us(void) {
    return model_now;
}

extern "C" void nx_stm32f1_usb_epr_write(volatile uint16_t* endpoint,
                                         uint16_t word) {
    const uint16_t live = *endpoint | inject_next_write;
    inject_next_write = 0U;
    last_word = word;
    const uint16_t kept = live & word & ctr_mask;
    const uint16_t setup =
        (kept & USB_EP_CTR_RX) != 0U ? live & USB_EP_SETUP : 0U;
    *endpoint =
        ((live ^ word) & toggle_mask) | (word & direct_mask) | kept | setup;
#if defined(NX_STM32F1_USB_PMA_EP1)
    if (model_ep1 != nullptr &&
        (endpoint == model_endpoint || endpoint == model_ep1)) {
        *model_status &=
            static_cast<uint16_t>(~(USB_ISTR_CTR | USB_ISTR_EP_ID));
        if ((*model_endpoint & ctr_mask) != 0U)
            *model_status |= USB_ISTR_CTR;
        else if ((*model_ep1 & ctr_mask) != 0U)
            *model_status |= USB_ISTR_CTR | 1U;
        return;
    }
#endif
    if (endpoint == model_endpoint) {
        if (kept != 0U) {
            *model_status |= USB_ISTR_CTR;
        } else {
            *model_status &= static_cast<uint16_t>(~USB_ISTR_CTR);
        }
    }
}

extern "C" void nx_stm32f1_usb_istr_write(volatile uint16_t* status,
                                          uint16_t word) {
    *status = (*status & ~istr_w0c) | (*status & word & istr_w0c);
}

namespace {

class Stm32F1UsbEp0 : public testing::Test {
  protected:
    USB_TypeDef registers{};
    std::array<uint16_t, NX_STM32F1_USB_PMA_BYTES> pma{};
    nx_stm32f1_ep0_config_t config{&registers, pma.data(), {64U, 8U, 72U, 8U}};
    nx_stm32f1_ep0_t state{};

    void SetUp() override {
        inject_next_write = 0U;
        last_word = 0U;
        model_endpoint = &registers.EP0R;
        model_status = &registers.ISTR;
        ASSERT_EQ(nx_stm32f1_usb_ep0_start(&state, &config), NX_SUCCESS);
    }

    void TearDown() override {
        model_endpoint = nullptr;
        model_status = nullptr;
    }

    void out_data(const std::array<uint8_t, 8>& bytes, uint16_t count,
                  bool setup, bool in_completion = false) {
        for (uint16_t index = 0U; index < bytes.size(); index += 2U) {
            pma[72U + index] = static_cast<uint16_t>(
                bytes[index] |
                (static_cast<uint16_t>(bytes[index + 1U]) << 8U));
        }
        pma[6] = 0x1000U | count;
        const uint16_t tx = in_completion ? USB_EP_CTR_TX : 0U;
        const uint16_t setup_bit = setup ? USB_EP_SETUP : 0U;
        registers.EP0R =
            USB_EP_CONTROL | both_nak | USB_EP_CTR_RX | setup_bit | tx;
    }

    void setup(uint8_t type, uint8_t method, uint8_t value, uint8_t length) {
        out_data({type, method, value, 0U, 0U, 0U, length, 0U}, 8U, true);
        ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 1U), NX_SUCCESS);
    }

    void in_ack() {
        registers.EP0R = USB_EP_CONTROL | both_nak | USB_EP_CTR_TX;
    }

    void pending_status(nx_usb_request_t& packet) {
        nx_request_initialize(&packet.base);
        ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_STATUS,
                                    state.setup.epoch, 50U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &packet, true, 2U),
                  NX_SUCCESS);
    }

    void result(nx_usb_request_t& packet, nx_result_t value, size_t length) {
        nx_result_t observed = NX_ERROR_STATE;
        size_t actual = 42U;
        ASSERT_EQ(nx_request_result(&packet.base, &observed, &actual),
                  NX_SUCCESS);
        EXPECT_EQ(observed, value);
        EXPECT_EQ(actual, length);
    }
};

TEST(Stm32F1UsbEpr, ClearsOnlyObservedCtrAndPreservesUnrelatedToggles) {
    const uint16_t snapshot = USB_EP_CONTROL | USB_EP_TX_NAK | USB_EP_RX_VALID |
                              USB_EP_DTOG_RX | USB_EP_SETUP | USB_EP_CTR_TX;
    const uint16_t word = nx_stm32f1_usb_epr_word(
        snapshot, ctr_mask, USB_EPTX_STAT, USB_EP_TX_VALID);
    EXPECT_EQ(word, USB_EP_CONTROL | USB_EP_CTR_RX | USB_EPTX_DTOG1);
    uint16_t live = snapshot & ~USB_EP_SETUP;
    inject_next_write = USB_EP_CTR_RX;
    nx_stm32f1_usb_epr_write(&live, word);
    EXPECT_EQ(live, USB_EP_CONTROL | USB_EP_CTR_RX | USB_EP_TX_VALID |
                        USB_EP_RX_VALID | USB_EP_DTOG_RX);

    const uint16_t other = nx_stm32f1_usb_epr_word(USB_EP_CONTROL | ctr_mask,
                                                   USB_EP_CTR_RX, 0U, 0U);
    EXPECT_EQ(other, USB_EP_CONTROL | USB_EP_CTR_TX);
}

TEST_F(Stm32F1UsbEp0, StagesAddressUntilHostAcknowledgesStatusIn) {
    setup(0U, 5U, 42U, 0U);
    EXPECT_EQ(state.setup.epoch, 1U);
    EXPECT_EQ(nx_stm32f1_usb_ep0_address(&state, 2U, 42U), NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32f1_usb_ep0_address(&state, 1U, 41U), NX_ERROR_STATE);
    ASSERT_EQ(nx_stm32f1_usb_ep0_address(&state, 1U, 42U), NX_SUCCESS);
    nx_usb_request_t packet{};
    pending_status(packet);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(pma[2], 0U);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    EXPECT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_ERROR_BUSY);
    in_ack();
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 4U), NX_SUCCESS);
    result(packet, NX_SUCCESS, 0U);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF | 42U);
    EXPECT_EQ(registers.EP0R & ctr_mask, 0);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_IDLE);
}

TEST_F(Stm32F1UsbEp0, ReplacementSetupWinsWithSimultaneousStatusAcknowledge) {
    setup(0U, 5U, 25U, 0U);
    ASSERT_EQ(nx_stm32f1_usb_ep0_address(&state, 1U, 25U), NX_SUCCESS);
    nx_usb_request_t status{};
    pending_status(status);
    out_data({0x80U, 6U, 0x12U, 0x34U, 0x56U, 0x78U, 8U, 0U}, 8U, true, true);
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(status, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    EXPECT_EQ(state.setup.epoch, 2U);
    EXPECT_EQ(state.setup.value, 0x3412U);
    EXPECT_EQ(state.setup.index, 0x7856U);
    EXPECT_EQ(state.setup.length, 8U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_DATA_IN);
}

TEST_F(Stm32F1UsbEp0, SetupArrivingDuringCtrWriteRemainsVisible) {
    setup(0U, 5U, 19U, 0U);
    ASSERT_EQ(nx_stm32f1_usb_ep0_address(&state, 1U, 19U), NX_SUCCESS);
    nx_usb_request_t status{};
    pending_status(status);
    out_data({0x80U, 6U, 0U, 1U, 0U, 0U, 8U, 0U}, 8U, true);
    in_ack();
    inject_next_write = setup_flags;
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(status, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    EXPECT_EQ(state.setup.epoch, 2U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_DATA_IN);
}

TEST_F(Stm32F1UsbEp0, ShortInThenZeroOutFinishesControlWithoutFalseSettlement) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[] = {1U, 2U, 3U};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    EXPECT_EQ(pma[64], 0x0201U);
    EXPECT_EQ(pma[66], 0x0003U);
    EXPECT_EQ(nx_request_state(&data.base), NX_REQUEST_ACTIVE);
    in_ack();
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(data, NX_SUCCESS, 3U);

    nx_usb_request_t status{};
    nx_request_initialize(&status.base);
    ASSERT_EQ(nx_usb_out_prepare(&status, nullptr, 0U, NX_USB_STATUS, 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &status, false, 4U),
              NX_SUCCESS);
    EXPECT_NE(registers.EP0R & USB_EP_KIND, 0U);
    out_data({}, 0U, false);
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 5U), NX_SUCCESS);
    result(status, NX_SUCCESS, 0U);
    EXPECT_EQ(registers.EP0R & USB_EP_KIND, 0U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_IDLE);
}

TEST_F(Stm32F1UsbEp0, RequiresFullOutDataBeforeStatusInAndRejectsWrongEpoch) {
    setup(0x40U, 50U, 0U, 3U);
    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    uint8_t buffer[3] = {};
    ASSERT_EQ(nx_usb_out_prepare(&packet, buffer, sizeof(buffer), NX_USB_DATA,
                                 2U, 50U),
              NX_SUCCESS);
    EXPECT_EQ(nx_stm32f1_usb_ep0_submit(&state, &packet, false, 2U),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_READY);
    ASSERT_EQ(nx_usb_out_prepare(&packet, buffer, sizeof(buffer), NX_USB_DATA,
                                 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &packet, false, 2U),
              NX_SUCCESS);
    out_data({1U, 2U, 3U}, 3U, false);
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(packet, NX_SUCCESS, 3U);
    EXPECT_EQ(buffer[0], 1U);
    EXPECT_EQ(buffer[2], 3U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_STATUS_IN);
}

TEST_F(Stm32F1UsbEp0, FullDataInRequiresExplicitTerminatingPacket) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[8] = {};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    in_ack();
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(data, NX_SUCCESS, sizeof(bytes));
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_DATA_IN);
    ASSERT_EQ(nx_usb_in_prepare(&data, nullptr, 0U, NX_USB_DATA, 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 4U), NX_SUCCESS);
    EXPECT_EQ(pma[2], 0U);
    EXPECT_EQ(nx_request_state(&data.base), NX_REQUEST_ACTIVE);
    in_ack();
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 5U), NX_SUCCESS);
    result(data, NX_SUCCESS, 0U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_STATUS_OUT);
}

TEST_F(Stm32F1UsbEp0, CancellationRetainsUntilHardwareResetAndFunctionDisable) {
    setup(0U, 9U, 1U, 0U);
    nx_usb_request_t status{};
    pending_status(status);
    ASSERT_EQ(nx_stm32f1_usb_ep0_cancel(&state, &status), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&status.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(nx_stm32f1_usb_ep0_service(&state, 4U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_stm32f1_usb_ep0_close(&state), NX_ERROR_BUSY);
    registers.CNTR = USB_CNTR_FRES;
    registers.EP0R = USB_EP_TX_VALID | USB_EP_CTR_TX;
    registers.DADDR = 0U;
    EXPECT_EQ(nx_stm32f1_usb_ep0_close(&state), NX_ERROR_BUSY);
    registers.EP0R = 0U;
    registers.DADDR = USB_DADDR_EF;
    EXPECT_EQ(nx_stm32f1_usb_ep0_close(&state), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&status.base), NX_REQUEST_DRAINING);
    registers.DADDR = 0U;
    ASSERT_EQ(nx_stm32f1_usb_ep0_close(&state), NX_SUCCESS);
    result(status, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(Stm32F1UsbEp0, TimeoutQuarantinesUntilAckAndCannotChangeAddress) {
    setup(0U, 5U, 11U, 0U);
    ASSERT_EQ(nx_stm32f1_usb_ep0_address(&state, 1U, 11U), NX_SUCCESS);
    nx_usb_request_t status{};
    pending_status(status);
    EXPECT_EQ(nx_stm32f1_usb_ep0_service(&state, 50U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&status.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    in_ack();
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 51U), NX_SUCCESS);
    result(status, NX_ERROR_TIMEOUT, 0U);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
}

TEST_F(Stm32F1UsbEp0, MalformedSetupCannotPublishNewEpoch) {
    out_data({0U, 5U, 2U}, 7U, true);
    EXPECT_EQ(nx_stm32f1_usb_ep0_service(&state, 1U), NX_ERROR_IO);
    EXPECT_EQ(state.setup.epoch, 0U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_IDLE);
    EXPECT_EQ(registers.EP0R & ctr_mask, 0);
    setup(0x80U, 6U, 1U, 8U);
    EXPECT_EQ(state.setup.epoch, 1U);
}

TEST_F(Stm32F1UsbEp0, EarlyStatusOutClosesAdmissionButRetainsArmedDataIn) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t status{};
    nx_request_initialize(&status.base);
    ASSERT_EQ(nx_usb_out_prepare(&status, nullptr, 0U, NX_USB_STATUS, 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &status, false, 2U),
              NX_SUCCESS);

    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[8] = {};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    EXPECT_EQ(registers.EP0R,
              USB_EP_CONTROL | USB_EP_KIND | USB_EP_RX_VALID | USB_EP_TX_VALID);

    out_data({}, 0U, false);
    registers.EP0R = USB_EP_CONTROL | USB_EP_KIND | USB_EP_RX_NAK |
                     USB_EP_TX_VALID | USB_EP_CTR_RX;
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(status, NX_SUCCESS, 0U);
    EXPECT_EQ(nx_request_state(&data.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_IDLE);

    nx_usb_request_t next{};
    nx_request_initialize(&next.base);
    ASSERT_EQ(
        nx_usb_in_prepare(&next, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    EXPECT_EQ(nx_stm32f1_usb_ep0_submit(&state, &next, true, 4U),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&next.base), NX_REQUEST_READY);
    in_ack();
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 5U), NX_SUCCESS);
    result(data, NX_ERROR_CANCELLED, 0U);
}

TEST_F(Stm32F1UsbEp0, DataInAckDoesNotWithdrawPrearmedStatusOut) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[3] = {};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    nx_usb_request_t status{};
    nx_request_initialize(&status.base);
    ASSERT_EQ(nx_usb_out_prepare(&status, nullptr, 0U, NX_USB_STATUS, 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &status, false, 2U),
              NX_SUCCESS);
    registers.EP0R = USB_EP_CONTROL | USB_EP_KIND | USB_EP_TX_NAK |
                     USB_EP_RX_VALID | USB_EP_CTR_TX;
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 3U), NX_SUCCESS);
    result(data, NX_SUCCESS, 3U);
    EXPECT_EQ(nx_request_state(&status.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(registers.EP0R,
              USB_EP_CONTROL | USB_EP_KIND | USB_EP_TX_NAK | USB_EP_RX_VALID);
    out_data({}, 0U, false);
    ASSERT_EQ(nx_stm32f1_usb_ep0_service(&state, 4U), NX_SUCCESS);
    result(status, NX_SUCCESS, 0U);
}

TEST_F(Stm32F1UsbEp0,
       BusResetWaitsForDisabledSourcesBeforeWithdrawingBothLoans) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[8] = {};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    nx_usb_request_t status{};
    nx_request_initialize(&status.base);
    ASSERT_EQ(nx_usb_out_prepare(&status, nullptr, 0U, NX_USB_STATUS, 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &status, false, 2U),
              NX_SUCCESS);
    EXPECT_EQ(nx_stm32f1_usb_ep0_bus_reset(&state), NX_ERROR_STATE);
    registers.ISTR = USB_ISTR_RESET;
    EXPECT_EQ(nx_stm32f1_usb_ep0_bus_reset(&state), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&data.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(nx_request_state(&status.base), NX_REQUEST_ACTIVE);
    registers.EP0R = 0U;
    ASSERT_EQ(nx_stm32f1_usb_ep0_bus_reset(&state), NX_SUCCESS);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    result(data, NX_ERROR_CANCELLED, 0U);
    result(status, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(state.phase, NX_STM32F1_EP0_IDLE);
    setup(0x80U, 6U, 1U, 8U);
    EXPECT_EQ(state.setup.epoch, 2U);
}

TEST_F(Stm32F1UsbEp0, LowPriorityIrqMasksAndServicesExactlyOneCausePerBudget) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[8] = {};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    nx_usb_request_t status{};
    nx_request_initialize(&status.base);
    ASSERT_EQ(nx_usb_out_prepare(&status, nullptr, 0U, NX_USB_STATUS, 1U, 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &status, false, 2U),
              NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_lp_enable(&state), NX_SUCCESS);
    EXPECT_FALSE(nx_stm32f1_usb_lp_interrupt(&registers));
    registers.ISTR = USB_ISTR_CTR;
    registers.EP0R = USB_EP_CONTROL | both_nak | USB_EP_KIND | ctr_mask;
    pma[6] = 0x1000U;
    EXPECT_EQ(nx_stm32f1_usb_lp_service(&state, 3U, 1U), NX_ERROR_BUSY);
    EXPECT_TRUE(nx_stm32f1_usb_lp_interrupt(&registers));
    EXPECT_EQ(registers.CNTR, 0U);
    EXPECT_EQ(registers.EP0R & ctr_mask, static_cast<int>(ctr_mask));

    EXPECT_EQ(nx_stm32f1_usb_lp_service(&state, 3U, 1U), NX_ERROR_BUSY);
    result(status, NX_SUCCESS, 0U);
    EXPECT_EQ(nx_request_state(&data.base), NX_REQUEST_DRAINING);
    EXPECT_NE(registers.EP0R & USB_EP_CTR_TX, 0U);
    EXPECT_NE(registers.ISTR & USB_ISTR_CTR, 0U);
    EXPECT_EQ(nx_stm32f1_usb_lp_enable(&state), NX_ERROR_BUSY);
    ASSERT_EQ(nx_stm32f1_usb_lp_service(&state, 4U, 1U), NX_SUCCESS);
    result(data, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(registers.ISTR & USB_ISTR_CTR, 0U);
    EXPECT_EQ(nx_stm32f1_usb_lp_enable(&state), NX_SUCCESS);
}

TEST_F(Stm32F1UsbEp0, LowPriorityResetPreservesAnUnownedErrorLatch) {
    ASSERT_EQ(nx_stm32f1_usb_lp_enable(&state), NX_SUCCESS);
    registers.ISTR = USB_ISTR_RESET | USB_ISTR_ERR;
    EXPECT_TRUE(nx_stm32f1_usb_lp_interrupt(&registers));
    EXPECT_EQ(nx_stm32f1_usb_lp_service(&state, 2U, 1U), NX_ERROR_BUSY);
    EXPECT_NE(registers.ISTR & USB_ISTR_RESET, 0U);
    registers.EP0R = 0U;
    EXPECT_EQ(nx_stm32f1_usb_lp_service(&state, 3U, 1U), NX_SUCCESS);
    EXPECT_EQ(registers.ISTR, USB_ISTR_ERR);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
}

TEST_F(Stm32F1UsbEp0, LowPriorityCloseRequiresAllSourcesWithdrawn) {
    setup(0U, 9U, 1U, 0U);
    nx_usb_request_t status{};
    pending_status(status);
    registers.CNTR = USB_CNTR_FRES | USB_CNTR_WKUPM;
    registers.DADDR = 0U;
    registers.EP0R = 0U;
    EXPECT_EQ(nx_stm32f1_usb_lp_close(&state), NX_ERROR_BUSY);
    registers.CNTR = USB_CNTR_FRES;
    registers.ISTR = USB_ISTR_CTR;
    EXPECT_EQ(nx_stm32f1_usb_lp_close(&state), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&status.base), NX_REQUEST_ACTIVE);
    registers.ISTR = USB_ISTR_RESET;
    ASSERT_EQ(nx_stm32f1_usb_lp_close(&state), NX_SUCCESS);
    result(status, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(registers.ISTR, 0U);
}

TEST_F(Stm32F1UsbEp0, UnexpectedEndpointDoesNotClearAnotherInterruptOwner) {
    registers.ISTR = USB_ISTR_CTR | 1U;
    registers.EP0R = USB_EP_CTR_TX;
    EXPECT_EQ(nx_stm32f1_usb_lp_service(&state, 1U, 1U), NX_ERROR_IO);
    EXPECT_EQ(registers.ISTR, USB_ISTR_CTR | 1U);
    EXPECT_EQ(registers.EP0R, USB_EP_CTR_TX);
}

struct LinkGpio {
    bool pullup = true;
    bool vbus = true;
    bool fail_disconnect = false;
    unsigned drive_calls = 0U;

    static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
        auto& self = *static_cast<LinkGpio*>(context);
        self.drive_calls++;
        if ((set & 1U) != 0U) {
            self.pullup = true;
        } else if ((reset & 1U) != 0U && !self.fail_disconnect) {
            self.pullup = false;
        }
        return NX_SUCCESS;
    }

    static nx_result_t read(const void* context, uint32_t* value) {
        const auto& self = *static_cast<const LinkGpio*>(context);
        *value = (self.pullup ? 1U : 0U) | (self.vbus ? 2U : 0U);
        return NX_SUCCESS;
    }
};

class Stm32F103UsbLink : public testing::Test {
  protected:
    static constexpr uint32_t irq = 1U << NX_STM32F1_USB_LP_IRQ;
    USB_TypeDef registers{};
    RCC_TypeDef rcc{};
    NVIC_Type nvic{};
    std::array<uint16_t, NX_STM32F1_USB_PMA_BYTES> pma{};
    nx_gpio_ops_t operations{LinkGpio::write, LinkGpio::read, nullptr};
    LinkGpio board{};
    nx_gpio_port_t gpio{&operations, &board};
    nx_gpio_signal_t pullup{&gpio, 1U, true};
    nx_gpio_signal_t vbus{&gpio, 2U, true};
    nx_stm32f1_usb_link_config_t config{
        {&registers, pma.data(), {64U, 64U, 128U, 64U}},
        &rcc,
        &nvic,
        &vbus,
        &pullup};
    nx_stm32f1_usb_link_t state{};

    void SetUp() override {
        model_now = 100U;
        model_endpoint = &registers.EP0R;
        model_status = &registers.ISTR;
        rcc.CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        rcc.CFGR = RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PLLSRC;
#if defined(STM32F102xB)
        rcc.CFGR |= RCC_CFGR_PLLMULL6 | RCC_CFGR_USBPRE;
#else
        rcc.CR |= RCC_CR_HSEBYP;
        rcc.CFGR |= RCC_CFGR_PLLMULL9;
#endif
    }

    void TearDown() override {
        model_endpoint = nullptr;
        model_status = nullptr;
    }

    void attach() {
        ASSERT_EQ(nx_stm32f1_usb_link_begin(&state, &config), NX_SUCCESS);
        nvic.ISER[0] = irq;
        model_now = 102U;
        ASSERT_EQ(nx_stm32f1_usb_link_connect(&state), NX_SUCCESS);
    }
};

TEST_F(Stm32F103UsbLink, RejectsWrongPrescalerAndSharedCanBeforeBoardEffects) {
    const uint32_t reviewed = rcc.CFGR;
    rcc.CFGR ^= RCC_CFGR_USBPRE;
    EXPECT_EQ(nx_stm32f1_usb_link_begin(&state, &config), NX_ERROR_STATE);
    rcc.CFGR = reviewed & ~static_cast<uint32_t>(RCC_CFGR_PPRE1);
    EXPECT_EQ(nx_stm32f1_usb_link_begin(&state, &config), NX_ERROR_STATE);
    rcc.CFGR = reviewed;
    rcc.CR ^= RCC_CR_HSEBYP;
    EXPECT_EQ(nx_stm32f1_usb_link_begin(&state, &config), NX_ERROR_STATE);
    rcc.CR ^= RCC_CR_HSEBYP;
#if defined(RCC_APB1ENR_CAN1EN)
    rcc.APB1ENR = RCC_APB1ENR_CAN1EN;
    EXPECT_EQ(nx_stm32f1_usb_link_begin(&state, &config), NX_ERROR_STATE);
    EXPECT_EQ(rcc.APB1ENR, RCC_APB1ENR_CAN1EN);
#endif
    EXPECT_EQ(board.drive_calls, 0U);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(Stm32F103UsbLink, RequiresWarmUpVbusAndPublishedLpBeforeAttaching) {
    ASSERT_EQ(nx_stm32f1_usb_link_begin(&state, &config), NX_SUCCESS);
    EXPECT_FALSE(board.pullup);
    EXPECT_EQ(registers.CNTR, USB_CNTR_FRES);
    EXPECT_EQ(state.ready_at, 102U);
    nvic.ISER[0] = irq;
    model_now = 101U;
    EXPECT_EQ(nx_stm32f1_usb_link_connect(&state), NX_ERROR_BUSY);
    EXPECT_EQ(registers.DADDR, 0U);
    board.vbus = false;
    model_now = 102U;
    EXPECT_EQ(nx_stm32f1_usb_link_connect(&state), NX_ERROR_STATE);
    EXPECT_FALSE(board.pullup);
    board.vbus = true;
    nvic.ISER[0] = 0U;
    EXPECT_EQ(nx_stm32f1_usb_link_connect(&state), NX_ERROR_BUSY);
    nvic.ISER[0] = irq;
    ASSERT_EQ(nx_stm32f1_usb_link_connect(&state), NX_SUCCESS);
    EXPECT_TRUE(board.pullup);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    EXPECT_EQ(registers.CNTR, lp_sources);
}

TEST_F(Stm32F103UsbLink, FailedBoardDisconnectKeepsControllerAndClockAlive) {
    attach();
    board.fail_disconnect = true;
    EXPECT_EQ(nx_stm32f1_usb_link_stop(&state), NX_ERROR_IO);
    EXPECT_TRUE(board.pullup);
    EXPECT_EQ(registers.CNTR, lp_sources);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    EXPECT_EQ(state.config, &config);
}

TEST_F(Stm32F103UsbLink, StopsAfterDigitalDetachIrqJoinAndResetReadback) {
    attach();
    EXPECT_EQ(nx_stm32f1_usb_link_stop(&state), NX_ERROR_BUSY);
    EXPECT_FALSE(board.pullup);
    EXPECT_EQ(registers.CNTR, USB_CNTR_FRES);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    nvic.ISER[0] = 0U;
    nvic.ISPR[0] = irq;
    registers.EP0R = 0U;
    EXPECT_EQ(nx_stm32f1_usb_link_stop(&state), NX_ERROR_BUSY);
    nvic.ISPR[0] = 0U;
    registers.ISTR = USB_ISTR_CTR;
    EXPECT_EQ(nx_stm32f1_usb_link_stop(&state), NX_ERROR_BUSY);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    registers.ISTR = USB_ISTR_RESET;
    ASSERT_EQ(nx_stm32f1_usb_link_stop(&state), NX_SUCCESS);
    EXPECT_EQ(registers.CNTR, USB_CNTR_FRES | USB_CNTR_PDWN);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    EXPECT_EQ(state.config, nullptr);
}

TEST_F(Stm32F1UsbEp0, ActualSetupAndResetProduceSnapshotAndTimestampedEvent) {
    nx_usb_device_state_t snapshot{};
    ASSERT_EQ(nx_stm32f1_usb_ep0_snapshot(&state, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.speed, NX_USB_DEVICE_SPEED_UNKNOWN);
    EXPECT_FALSE(snapshot.control_pending);
    nx_usb_event_t event{};
    event.timestamp_us = 777U;
    EXPECT_EQ(nx_stm32f1_usb_lp_event(&state, 4U, &event), NX_ERROR_EMPTY);
    EXPECT_EQ(event.timestamp_us, 777U);

    out_data({0x80U, 6U, 0U, 1U, 0U, 0U, 8U, 0U}, 8U, true);
    registers.ISTR = USB_ISTR_CTR;
    ASSERT_EQ(nx_stm32f1_usb_lp_event(&state, 7U, &event), NX_SUCCESS);
    EXPECT_EQ(event.kind, NX_USB_EVENT_SETUP);
    EXPECT_EQ(event.timestamp_us, 7U);
    EXPECT_EQ(event.setup.epoch, 1U);
    EXPECT_EQ(event.setup.request, 6U);
    ASSERT_EQ(nx_stm32f1_usb_ep0_snapshot(&state, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.speed, NX_USB_DEVICE_FULL_SPEED);
    EXPECT_TRUE(snapshot.control_pending);
    EXPECT_EQ(snapshot.setup.value, 256U);

    registers.ISTR = USB_ISTR_RESET;
    EXPECT_EQ(nx_stm32f1_usb_lp_event(&state, 9U, &event), NX_ERROR_BUSY);
    EXPECT_EQ(event.kind, NX_USB_EVENT_SETUP);
    registers.EP0R = 0U;
    ASSERT_EQ(nx_stm32f1_usb_lp_event(&state, 10U, &event), NX_SUCCESS);
    EXPECT_EQ(event.kind, NX_USB_EVENT_RESET);
    EXPECT_EQ(event.timestamp_us, 10U);
    EXPECT_EQ(event.setup.epoch, 0U);
    ASSERT_EQ(nx_stm32f1_usb_ep0_snapshot(&state, &snapshot), NX_SUCCESS);
    EXPECT_EQ(snapshot.speed, NX_USB_DEVICE_SPEED_UNKNOWN);
    EXPECT_EQ(snapshot.epoch, 1U);
    EXPECT_FALSE(snapshot.control_pending);
    EXPECT_EQ(snapshot.address, 0U);
}

TEST_F(Stm32F1UsbEp0,
       StallsOnlyQuiescentDirectionAndLeavesSetupRecoveryPossible) {
    setup(0x80U, 6U, 1U, 16U);
    nx_usb_request_t data{};
    nx_request_initialize(&data.base);
    const uint8_t bytes[3] = {};
    ASSERT_EQ(
        nx_usb_in_prepare(&data, bytes, sizeof(bytes), NX_USB_DATA, 1U, 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_usb_ep0_submit(&state, &data, true, 2U), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f1_usb_ep0_stall(&state, true, true), NX_ERROR_BUSY);
    EXPECT_EQ(registers.EP0R & USB_EPTX_STAT, USB_EP_TX_VALID);
    ASSERT_EQ(nx_stm32f1_usb_ep0_stall(&state, false, true), NX_SUCCESS);
    EXPECT_EQ(registers.EP0R & USB_EPRX_STAT, USB_EP_RX_STALL);
    EXPECT_EQ(nx_request_state(&data.base), NX_REQUEST_DRAINING);
    setup(0x80U, 6U, 2U, 8U);
    result(data, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(registers.EP0R & USB_EPRX_STAT, USB_EP_RX_NAK);
    EXPECT_EQ(state.setup.epoch, 2U);
    ASSERT_EQ(nx_stm32f1_usb_ep0_stall(&state, true, true), NX_SUCCESS);
    EXPECT_EQ(registers.EP0R & USB_EPTX_STAT, USB_EP_TX_STALL);
    ASSERT_EQ(nx_stm32f1_usb_ep0_stall(&state, true, false), NX_SUCCESS);
    EXPECT_EQ(registers.EP0R & USB_EPTX_STAT, USB_EP_TX_NAK);
}

} /* namespace */

extern "C" void nx_stm32f1_usb_lp_nvic_enable(NVIC_Type* nvic) {
    nvic->ISER[0] |= 1U << NX_STM32F1_USB_LP_IRQ;
}

extern "C" void nx_stm32f1_usb_lp_nvic_disable(NVIC_Type* nvic) {
    nvic->ISER[0] &= ~(1U << NX_STM32F1_USB_LP_IRQ);
}

extern "C" void nx_stm32f1_usb_lp_nvic_ack(NVIC_Type* nvic) {
    nvic->ISPR[0] &= ~(1U << NX_STM32F1_USB_LP_IRQ);
}

namespace {

class Stm32F103UsbDevice : public Stm32F103UsbLink {
  protected:
    std::array<nx_usb_event_t, 2U> events{};
    nx_stm32f1_usb_pma_device_config_t device_config {
        config, events.data(), static_cast<uint8_t>(events.size()), 5U
#if defined(NX_STM32F1_USB_PMA_EP1)
            ,
            USB_EP_BULK, {
            192U, 64U, 256U, 64U
        }
#endif
    };
    nx_stm32f1_usb_pma_device_t device{};
    nx_stm32f1_usb_pma_endpoint_face_t ep0_in {
        &device, true
#if defined(NX_STM32F1_USB_PMA_EP1)
            ,
            0U
#endif
    };
    nx_stm32f1_usb_pma_endpoint_face_t ep0_out {
        &device, false
#if defined(NX_STM32F1_USB_PMA_EP1)
            ,
            0U
#endif
    };
    nx_usb_device_port_t port{&nx_stm32f1_usb_pma_device_ops, &device};
    nx_usb_endpoint_port_t in{&nx_stm32f1_usb_pma_endpoint_ops, &ep0_in};
    nx_usb_endpoint_port_t out{&nx_stm32f1_usb_pma_endpoint_ops, &ep0_out};

    void attach_device() {
        ASSERT_EQ(nx_stm32f1_usb_pma_device_begin(&device, &device_config),
                  NX_SUCCESS);
        EXPECT_FALSE(board.pullup);
        EXPECT_EQ(nx_stm32f1_usb_pma_device_connect(&device), NX_ERROR_BUSY);
        model_now += 2U;
        ASSERT_EQ(nx_stm32f1_usb_pma_device_connect(&device), NX_SUCCESS);
        EXPECT_TRUE(board.pullup);
    }

    void publish_setup(uint8_t request, uint8_t value) {
        pma[128] = static_cast<uint16_t>(request << 8U);
        pma[130] = value;
        pma[132] = 0U;
        pma[134] = 0U;
        pma[6] = 0x8408U;
        registers.EP0R = USB_EP_CONTROL | both_nak | setup_flags;
        registers.ISTR = USB_ISTR_CTR;
        nx_stm32f1_usb_pma_device_irq(&device);
        EXPECT_EQ(registers.CNTR & USB_CNTR_CTRM, 0U);
        ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    }

    void publish_bus(uint16_t flag, uint16_t line) {
        registers.FNR = line;
        registers.ISTR = flag;
        nx_stm32f1_usb_pma_device_irq(&device);
        ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    }
};

TEST_F(Stm32F103UsbDevice, DispatchesLogicalEndpointAndAckedAddress) {
    attach_device();
    publish_setup(5U, 39U);
    std::array<nx_usb_event_t, 2U> observed{};
    size_t count = 0U;
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_ENUMERATED);
    EXPECT_EQ(observed[1].kind, NX_USB_EVENT_SETUP);
    EXPECT_EQ(observed[1].timestamp_us, model_now);
    uint64_t epoch = observed[1].setup.epoch;
    ASSERT_EQ(nx_usb_device_address(&port, epoch, 39U), NX_SUCCESS);
    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_STATUS, epoch,
                                model_now + 50U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&out, &packet), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &packet), NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_cancel(&out, &packet), NX_ERROR_STATE);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);

    registers.EP0R = USB_EP_CONTROL | both_nak | USB_EP_CTR_TX;
    registers.ISTR = USB_ISTR_CTR;
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF | 39U);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32F103UsbDevice, KeepsOlderEventsAndLatestSetupWhenReportingLoss) {
    attach_device();
    publish_setup(5U, 10U);
    ++model_now;
    publish_setup(5U, 11U);
    ++model_now;
    publish_setup(5U, 12U);
    nx_usb_device_state_t latest{};
    ASSERT_EQ(nx_usb_device_state(&port, &latest), NX_SUCCESS);
    EXPECT_EQ(latest.lost_events, 2U);
    EXPECT_EQ(latest.setup.value, 12U);
    EXPECT_EQ(latest.speed, NX_USB_DEVICE_FULL_SPEED);

    std::array<nx_usb_event_t, 3U> observed{};
    size_t count = 0U;
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), 1U, &count),
              NX_SUCCESS);
    EXPECT_EQ(count, 1U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_ENUMERATED);
    ++model_now;
    publish_setup(5U, 13U);
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_SETUP);
    EXPECT_EQ(observed[0].setup.value, 10U);
    EXPECT_EQ(observed[1].kind, NX_USB_EVENT_LOSS);
    EXPECT_EQ(observed[1].lost, 3U);
    EXPECT_EQ(observed[1].timestamp_us, model_now);
    EXPECT_EQ(nx_usb_device_read_events(&port, observed.data(), 1U, &count),
              NX_ERROR_EMPTY);
    EXPECT_EQ(count, 0U);
}

TEST_F(Stm32F103UsbDevice, ObservesSuspendAndARealResumeAfterSpuriousWake) {
    attach_device();
    publish_bus(USB_ISTR_SUSP, USB_FNR_RXDP);
    EXPECT_NE(registers.CNTR & USB_CNTR_FSUSP, 0U);
    EXPECT_EQ(registers.CNTR & USB_CNTR_LP_MODE, 0U);
    publish_bus(USB_ISTR_WKUP, USB_FNR_RXDP);
    EXPECT_NE(registers.CNTR & USB_CNTR_FSUSP, 0U);
    ++model_now;
    publish_bus(USB_ISTR_WKUP, USB_FNR_RXDM);
    EXPECT_EQ(registers.CNTR & USB_CNTR_FSUSP, 0U);
    std::array<nx_usb_event_t, 2U> observed{};
    size_t count;
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_SUSPEND);
    EXPECT_EQ(observed[1].kind, NX_USB_EVENT_RESUME);
    EXPECT_EQ(observed[1].timestamp_us, model_now);
}

TEST_F(Stm32F103UsbDevice,
       ResetFromSuspendProvidesResetWithoutSyntheticResume) {
    attach_device();
    publish_bus(USB_ISTR_SUSP, USB_FNR_RXDP);
    registers.EP0R = 0U;
    registers.DADDR = 0U;
    publish_bus(USB_ISTR_RESET | USB_ISTR_WKUP, 0U);
    EXPECT_EQ(registers.CNTR & USB_CNTR_FSUSP, 0U);
    EXPECT_EQ(registers.ISTR & (USB_ISTR_RESET | USB_ISTR_WKUP), 0U);
    std::array<nx_usb_event_t, 2U> observed{};
    size_t count;
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_SUSPEND);
    EXPECT_EQ(observed[1].kind, NX_USB_EVENT_RESET);
}

TEST_F(Stm32F103UsbDevice, KeepsAdmissionClosedBetweenSe0AndReset) {
    attach_device();
    publish_setup(5U, 41U);
    std::array<nx_usb_event_t, 2U> observed{};
    size_t count = 0U;
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);

    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_STATUS, 1U,
                                model_now + 50U),
              NX_SUCCESS);
    registers.ISTR = USB_ISTR_SUSP;
    EXPECT_EQ(nx_usb_endpoint_submit(&in, &packet), NX_ERROR_STATE);
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    publish_bus(USB_ISTR_WKUP, 0U);
    EXPECT_EQ(registers.CNTR & USB_CNTR_FSUSP, 0U);
    nx_usb_device_state_t snapshot{};
    ASSERT_EQ(nx_usb_device_state(&port, &snapshot), NX_SUCCESS);
    EXPECT_TRUE(snapshot.suspended);
    EXPECT_EQ(nx_usb_device_address(&port, 1U, 41U), NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_endpoint_submit(&in, &packet), NX_ERROR_STATE);
    EXPECT_EQ(nx_usb_endpoint_stall(&out, true), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_READY);

    registers.EP0R = 0U;
    registers.DADDR = 0U;
    publish_bus(USB_ISTR_RESET, USB_FNR_RXDP);
    ASSERT_EQ(nx_usb_device_state(&port, &snapshot), NX_SUCCESS);
    EXPECT_FALSE(snapshot.suspended);
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_SUSPEND);
    EXPECT_EQ(observed[1].kind, NX_USB_EVENT_RESET);
}

TEST_F(Stm32F103UsbDevice, Se0WithoutResetReturnsToSuspendUntilNewWake) {
    attach_device();
    publish_bus(USB_ISTR_SUSP, USB_FNR_RXDP);
    publish_bus(USB_ISTR_WKUP, 0U);
    registers.FNR = USB_FNR_RXDM;
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    EXPECT_NE(registers.CNTR & USB_CNTR_FSUSP, 0U);
    publish_bus(USB_ISTR_WKUP, USB_FNR_RXDM);
    EXPECT_EQ(registers.CNTR & USB_CNTR_FSUSP, 0U);
    std::array<nx_usb_event_t, 2U> observed{};
    size_t count = 0U;
    ASSERT_EQ(nx_usb_device_read_events(&port, observed.data(), observed.size(),
                                        &count),
              NX_SUCCESS);
    ASSERT_EQ(count, 2U);
    EXPECT_EQ(observed[0].kind, NX_USB_EVENT_SUSPEND);
    EXPECT_EQ(observed[1].kind, NX_USB_EVENT_RESUME);
}

TEST_F(Stm32F103UsbDevice, StopJoinsActualNvicAndRetainsLoanWhileActive) {
    nvic.IPR[NX_STM32F1_USB_LP_IRQ] = 0xA0U;
    attach_device();
    publish_setup(5U, 39U);
    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_STATUS,
                                device.link.endpoint.setup.epoch,
                                model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &packet), NX_SUCCESS);
    nvic.IABR[0] = irq;
    nvic.ISPR[0] = irq | 1U;
    EXPECT_EQ(nx_usb_device_stop(&port), NX_ERROR_BUSY);
    EXPECT_FALSE(board.pullup);
    EXPECT_NE(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_ACTIVE);

    nvic.IABR[0] = 0U;
    registers.EP0R = 0U;
    ASSERT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    EXPECT_EQ(nvic.ISER[0] & irq, 0U);
    EXPECT_EQ(nvic.ISPR[0], 1U);
    EXPECT_EQ(nvic.IPR[NX_STM32F1_USB_LP_IRQ], 0xA0U);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32F103UsbDevice, RejectsAliasedEventOutputWithoutChangingCount) {
    attach_device();
    publish_setup(5U, 25U);
    size_t count = 17U;
    EXPECT_EQ(nx_usb_device_read_events(&port, events.data(), 1U, &count),
              NX_ERROR_INVALID);
    EXPECT_EQ(count, 17U);
    auto* snapshot = reinterpret_cast<nx_usb_device_state_t*>(&device);
    EXPECT_EQ(nx_usb_device_state(&port, snapshot), NX_ERROR_INVALID);
    EXPECT_EQ(device.count, 2U);
}

TEST_F(Stm32F103UsbDevice, VbusWithdrawalDetachesAndReleasesPullup) {
    attach_device();
    registers.EP0R = 0U;
    board.vbus = false;
    EXPECT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    EXPECT_FALSE(board.pullup);
    EXPECT_EQ(nvic.ISER[0] & irq, 0U);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    nx_usb_device_state_t observed{};
    ASSERT_EQ(nx_usb_device_state(&port, &observed), NX_SUCCESS);
    EXPECT_FALSE(observed.started);
}

TEST_F(Stm32F103UsbDevice, ResetReadmitsEp0WithHardwareRetainedCtr) {
    attach_device();
    publish_setup(5U, 21U);
    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_STATUS,
                                device.link.endpoint.setup.epoch,
                                model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &packet), NX_SUCCESS);
    /* Hardware bus reset disables STAT but retains both old CTR latches. */
    registers.EP0R = ctr_mask;
    registers.DADDR = 0U;
    publish_bus(USB_ISTR_RESET | USB_ISTR_CTR, 0U);
    EXPECT_EQ(registers.ISTR & (USB_ISTR_RESET | USB_ISTR_CTR), 0U);
    EXPECT_EQ(registers.EP0R & ctr_mask, 0);
    EXPECT_EQ(registers.DADDR, USB_DADDR_EF);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_SETTLED);
}

TEST_F(Stm32F103UsbDevice, StopAcksOwnedCtrAfterHardwareDisablesEp0) {
    attach_device();
    publish_setup(5U, 28U);
    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_STATUS,
                                device.link.endpoint.setup.epoch,
                                model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&in, &packet), NX_SUCCESS);
    /* The model has not observed the FRES reset yet. */
    registers.EP0R = USB_EP_CONTROL | USB_EP_TX_VALID | USB_EP_CTR_RX;
    registers.ISTR = USB_ISTR_CTR;
    EXPECT_EQ(nx_usb_device_stop(&port), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_ACTIVE);
    registers.EP0R = ctr_mask;
    EXPECT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    EXPECT_EQ(registers.ISTR & USB_ISTR_CTR, 0U);
    EXPECT_EQ(registers.EP0R & ctr_mask, 0);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_SETTLED);
}
#if defined(NX_STM32F1_USB_PMA_EP1)
class Stm32F103UsbEp1 : public Stm32F103UsbDevice,
                        public ::testing::WithParamInterface<uint16_t> {
  protected:
    nx_stm32f1_usb_pma_endpoint_face_t ep1_in{&device, true, 1U};
    nx_stm32f1_usb_pma_endpoint_face_t ep1_out{&device, false, 1U};
    nx_usb_endpoint_port_t data_in{&nx_stm32f1_usb_pma_endpoint_ops, &ep1_in};
    nx_usb_endpoint_port_t data_out{&nx_stm32f1_usb_pma_endpoint_ops, &ep1_out};
    nx_usb_activation_port_t activation{&nx_stm32f1_usb_pma_ep1_activation_ops,
                                        &device};

    void SetUp() override {
        Stm32F103UsbDevice::SetUp();
        device_config.ep1_type = GetParam();
        model_ep1 = &registers.EP1R;
    }

    void TearDown() override {
        model_ep1 = nullptr;
        Stm32F103UsbDevice::TearDown();
    }

    void activate() {
        publish_setup(9U, 1U);
        nx_usb_request_t status{};
        nx_request_initialize(&status.base);
        ASSERT_EQ(nx_usb_in_prepare(&status, nullptr, 0U, NX_USB_STATUS, 1U,
                                    model_now + 100U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_endpoint_submit(&in, &status), NX_SUCCESS);
        registers.EP0R = USB_EP_CONTROL | both_nak | USB_EP_CTR_TX;
        registers.ISTR = USB_ISTR_CTR;
        ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
        ASSERT_EQ(nx_request_state(&status.base), NX_REQUEST_SETTLED);
        ASSERT_EQ(nx_usb_activation_set(&activation, 1U, 0x81U, true),
                  NX_SUCCESS);
        ASSERT_EQ(nx_usb_activation_set(&activation, 1U, 0x01U, true),
                  NX_SUCCESS);
    }

    void result(nx_usb_request_t& request, nx_result_t expected, size_t bytes) {
        nx_result_t observed = NX_ERROR_STATE;
        size_t count = 0U;
        ASSERT_EQ(nx_request_result(&request.base, &observed, &count),
                  NX_SUCCESS);
        EXPECT_EQ(observed, expected);
        EXPECT_EQ(count, bytes);
    }
};

TEST_P(Stm32F103UsbEp1, ArmsOnlyAnExplicitPairAfterControlCompletion) {
    attach_device();
    EXPECT_EQ(registers.EP1R & both_nak, 0);
    EXPECT_EQ(registers.EP1R & (USB_EP_TYPE_MASK | USB_EP_KIND),
              static_cast<unsigned>(GetParam()));
    EXPECT_EQ(nx_usb_activation_set(&activation, 0U, 0x81U, true),
              NX_ERROR_STATE);
    activate();
    EXPECT_EQ(registers.EP1R & both_nak, both_nak);
    EXPECT_EQ(pma[8U], 192U);
    EXPECT_EQ(pma[12U], 256U);
    EXPECT_EQ(nx_usb_activation_set(&activation, 1U, 0x82U, true),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_usb_activation_set(&activation, 1U, 0x81U, false), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & USB_EPRX_STAT, USB_EP_RX_NAK);
}

TEST_P(Stm32F103UsbEp1, IndependentlyCompletesOddInAndShortOut) {
    attach_device();
    activate();
    nx_usb_request_t tx{};
    nx_usb_request_t rx{};
    const std::array<uint8_t, 3U> source{1U, 2U, 3U};
    std::array<uint8_t, 4U> output{};
    nx_request_initialize(&tx.base);
    nx_request_initialize(&rx.base);
    ASSERT_EQ(nx_usb_in_prepare(&tx, source.data(), source.size(), NX_USB_DATA,
                                0U, model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_out_prepare(&rx, output.data(), output.size(), NX_USB_DATA,
                                 0U, model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&data_in, &tx), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&data_out, &rx), NX_SUCCESS);
    EXPECT_EQ(pma[192U], 0x0201U);
    EXPECT_EQ(pma[194U], 3U);
    pma[256U] = 0x0809U;
    pma[14U] = 0x8402U;
    registers.EP1R |= ctr_mask;
    registers.EP1R = (registers.EP1R &
                      static_cast<uint16_t>(~(USB_EPRX_STAT | USB_EPTX_STAT))) |
                     both_nak;
    registers.ISTR = USB_ISTR_CTR | 1U;
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    result(tx, NX_SUCCESS, 3U);
    result(rx, NX_SUCCESS, 2U);
    EXPECT_EQ(output[0], 9U);
    EXPECT_EQ(output[1], 8U);
    EXPECT_EQ(registers.ISTR & USB_ISTR_CTR, 0U);
}

TEST_P(Stm32F103UsbEp1, HaltPreservesSiblingAndStopDrainsRetainedCtr) {
    attach_device();
    activate();
    registers.EP1R |= USB_EP_DTOG_RX | USB_EP_DTOG_TX;
    EXPECT_EQ(nx_usb_endpoint_stall(&data_in, true), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & (USB_EPRX_STAT | USB_EP_DTOG_RX),
              USB_EP_RX_NAK | USB_EP_DTOG_RX);
    EXPECT_EQ(nx_usb_endpoint_stall(&data_in, false), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & (USB_EPTX_STAT | USB_EP_DTOG_TX), USB_EP_TX_NAK);

    nx_usb_request_t packet{};
    nx_request_initialize(&packet.base);
    ASSERT_EQ(nx_usb_in_prepare(&packet, nullptr, 0U, NX_USB_DATA, 0U,
                                model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&data_in, &packet), NX_SUCCESS);
    nvic.IABR[0] = irq;
    EXPECT_EQ(nx_usb_device_stop(&port), NX_ERROR_BUSY);
    EXPECT_FALSE(board.pullup);
    EXPECT_EQ(nx_request_state(&packet.base), NX_REQUEST_ACTIVE);
    nvic.IABR[0] = 0U;
    registers.EP0R = 0U;
    registers.EP1R = USB_EP_CTR_TX;
    registers.ISTR = USB_ISTR_CTR | 1U;
    ASSERT_EQ(nx_usb_device_stop(&port), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & ctr_mask, 0);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
    result(packet, NX_ERROR_CANCELLED, 0U);
}

TEST_P(Stm32F103UsbEp1, RejectsBusyDeactivationAndDrainsBothAfterReset) {
    attach_device();
    activate();
    nx_usb_request_t tx{};
    nx_usb_request_t rx{};
    std::array<uint8_t, 3U> bytes{};
    nx_request_initialize(&tx.base);
    nx_request_initialize(&rx.base);
    ASSERT_EQ(
        nx_usb_in_prepare(&tx, nullptr, 0U, NX_USB_DATA, 0U, model_now + 50U),
        NX_SUCCESS);
    ASSERT_EQ(nx_usb_out_prepare(&rx, bytes.data(), bytes.size(), NX_USB_DATA,
                                 0U, model_now + 50U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&data_in, &tx), NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&data_out, &rx), NX_SUCCESS);
    EXPECT_EQ(nx_usb_activation_set(&activation, 1U, 0x01U, false),
              NX_ERROR_BUSY);
    ASSERT_EQ(nx_usb_endpoint_cancel(&data_in, &tx), NX_SUCCESS);
    registers.EP0R = 0U;
    registers.DADDR = 0U;
    /* A task write crossing the hardware reset can leave STAT nonzero. */
    registers.EP1R = ctr_mask | USB_EP_TX_STALL;
    publish_bus(USB_ISTR_CTR | USB_ISTR_RESET | 1U, 0U);
    result(tx, NX_ERROR_CANCELLED, 0U);
    result(rx, NX_ERROR_CANCELLED, 0U);
    EXPECT_EQ(registers.EP1R, static_cast<unsigned>(GetParam() | 1U));
    EXPECT_EQ(nx_usb_activation_set(&activation, 1U, 0x01U, true),
              NX_ERROR_STATE);
}

TEST_P(Stm32F103UsbEp1, ExpiredOutRetainsCallerMemoryUntilHardwareWithdraws) {
    attach_device();
    activate();
    nx_usb_request_t rx{};
    std::array<uint8_t, 3U> payload{3U, 2U, 1U};
    nx_request_initialize(&rx.base);
    ASSERT_EQ(nx_usb_out_prepare(&rx, payload.data(), payload.size(),
                                 NX_USB_DATA, 0U, model_now + 1U),
              NX_SUCCESS);
    ASSERT_EQ(nx_usb_endpoint_submit(&data_out, &rx), NX_SUCCESS);
    ++model_now;
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&rx.base), NX_REQUEST_QUARANTINED);
    pma[256U] = 0xABCDU;
    pma[14U] = 0x8402U;
    registers.EP1R = (registers.EP1R & static_cast<uint16_t>(~USB_EPRX_STAT)) |
                     USB_EP_RX_NAK | USB_EP_CTR_RX;
    registers.ISTR = USB_ISTR_CTR | 1U;
    ASSERT_EQ(nx_usb_device_service(&port), NX_SUCCESS);
    result(rx, NX_ERROR_TIMEOUT, 0U);
    EXPECT_EQ(payload[0], 3U);
    EXPECT_EQ(payload[1], 2U);
}

INSTANTIATE_TEST_SUITE_P(SingleBuffered, Stm32F103UsbEp1,
                         ::testing::Values(USB_EP_BULK, USB_EP_INTERRUPT));
#endif

#if defined(NX_TEST_PMA_ST_DEVICE)
class Stm32F103StUsbDevice : public Stm32F103UsbDevice {
  protected:
    nx_stm32f1_usb_pma_endpoint_face_t f1_in{&device, true, 1U};
    nx_stm32f1_usb_pma_endpoint_face_t f1_out{&device, false, 1U};
    nx_usb_endpoint_port_t bulk_in{&nx_stm32f1_usb_pma_endpoint_ops, &f1_in};
    nx_usb_endpoint_port_t bulk_out{&nx_stm32f1_usb_pma_endpoint_ops, &f1_out};
    nx_usb_activation_port_t activation{&nx_stm32f1_usb_pma_ep1_activation_ops,
                                        &device};
    const nx_usb_endpoint_profile_t endpoints[4]{
        {&in, 64U, 0U, true, NX_USB_PROFILE_CONTROL},
        {&out, 64U, 0U, false, NX_USB_PROFILE_CONTROL},
        {&bulk_in, 64U, 1U, true, NX_USB_PROFILE_BULK},
        {&bulk_out, 64U, 1U, false, NX_USB_PROFILE_BULK}};
    const nx_usb_device_profile_t profile{&port,
                                          endpoints,
                                          4U,
                                          48000000U,
                                          0U,
                                          0U,
                                          8U,
                                          NX_USB_PROFILE_DEVICE,
                                          NX_USB_PROFILE_FULL_SPEED,
                                          NX_USB_PROFILE_INTERNAL_FS,
                                          NX_USB_PROFILE_PACKET_MEMORY,
                                          {nullptr, nullptr}};

    void SetUp() override {
        Stm32F103UsbDevice::SetUp();
        model_ep1 = &registers.EP1R;
    }

    void TearDown() override {
        model_ep1 = nullptr;
        Stm32F103UsbDevice::TearDown();
    }

    void setup_request(uint8_t type, uint8_t request, uint16_t value,
                       uint16_t length) {
        pma[128U] = static_cast<uint16_t>((request << 8U) | type);
        pma[130U] = value;
        pma[132U] = 0U;
        pma[134U] = length;
        pma[6U] = 0x8408U;
        registers.EP0R = USB_EP_CONTROL | both_nak | setup_flags;
        registers.ISTR = USB_ISTR_CTR;
        ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
        /* ENUMERATED precedes the first SETUP. */
        ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
    }

    void acknowledged_ep0_in() {
        registers.EP0R =
            (registers.EP0R & static_cast<uint16_t>(~USB_EPTX_STAT)) |
            USB_EP_TX_NAK | USB_EP_CTR_TX;
        registers.ISTR = USB_ISTR_CTR;
        ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
    }

    void acknowledged_ep0_out() {
        pma[6U] = 0x8400U;
        registers.EP0R =
            (registers.EP0R & static_cast<uint16_t>(~USB_EPRX_STAT)) |
            USB_EP_RX_NAK | USB_EP_CTR_RX;
        registers.ISTR = USB_ISTR_CTR;
        ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
    }
};

TEST_F(Stm32F103StUsbDevice, RunsOfficialEnumerationVendorEchoAndReset) {
    attach_device();
    nx_usb_device_profile_t invalid = profile;
    invalid.fifo_words = 320U;
    EXPECT_EQ(nx_usb_profile_validate(&invalid), NX_ERROR_INVALID);
    ASSERT_EQ(nx_test_pma_st_start(&profile, &activation), NX_SUCCESS);
    setup_request(0U, 5U, 41U, 0U);
    EXPECT_EQ(registers.DADDR & USB_DADDR_ADD, 0U);
    acknowledged_ep0_in();
    EXPECT_EQ(registers.DADDR & USB_DADDR_ADD, 41U);

    setup_request(0x80U, 6U, 0x0200U, 32U);
    EXPECT_EQ(pma[2U], 32U);
    EXPECT_EQ(pma[64U], 0x0209U);
    EXPECT_EQ(pma[70U], 0xC000U);
    acknowledged_ep0_in();
    EXPECT_EQ(registers.EP0R & USB_EPRX_STAT, USB_EP_RX_VALID);
    EXPECT_EQ(registers.EP0R & USB_EPTX_STAT, USB_EP_TX_STALL);
    acknowledged_ep0_out();
    EXPECT_EQ(device.link.endpoint.phase, NX_STM32F1_EP0_IDLE);

    setup_request(0U, 9U, 1U, 0U);
    EXPECT_EQ(registers.EP1R & both_nak, both_nak);
    nx_usb_request_t early{};
    nx_request_initialize(&early.base);
    ASSERT_EQ(nx_usb_in_prepare(&early, nullptr, 0U, NX_USB_DATA, 0U,
                                model_now + 50U),
              NX_SUCCESS);
    EXPECT_EQ(nx_usb_endpoint_submit(&bulk_in, &early), NX_ERROR_STATE);
    acknowledged_ep0_in();
    EXPECT_EQ(registers.EP1R & USB_EPRX_STAT, USB_EP_RX_VALID);
    pma[256U] = 0x0201U;
    pma[258U] = 0x0003U;
    pma[14U] = 0x8403U;
    registers.EP1R = 1U | both_nak | USB_EP_CTR_RX;
    registers.ISTR = USB_ISTR_CTR | 1U;
    ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & USB_EPTX_STAT, USB_EP_TX_VALID);
    EXPECT_EQ(pma[10U], 3U);
    EXPECT_EQ(pma[192U], 0x0201U);
    EXPECT_EQ(pma[194U] & 0xffU, 3U);
    registers.EP1R = 1U | both_nak | USB_EP_CTR_TX;
    registers.ISTR = USB_ISTR_CTR | 1U;
    ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & USB_EPRX_STAT, USB_EP_RX_VALID);

    registers.EP0R = 0U;
    registers.EP1R = USB_EP_CTR_RX;
    registers.DADDR = 0U;
    registers.ISTR = USB_ISTR_RESET | USB_ISTR_CTR | 1U;
    ASSERT_EQ(nx_test_pma_st_step(), NX_SUCCESS);
    EXPECT_EQ(registers.EP1R & (ctr_mask | both_nak), 0);
    EXPECT_EQ(nx_test_pma_st_stop(), NX_ERROR_BUSY);
    ASSERT_NE(registers.CNTR & USB_CNTR_FRES, 0U);
    /* Force reset disables both STAT fields in silicon. */
    registers.EP0R = 0U;
    ASSERT_EQ(nx_test_pma_st_stop(), NX_SUCCESS);
    EXPECT_FALSE(board.pullup);
    EXPECT_EQ(rcc.APB1ENR & RCC_APB1ENR_USBEN, 0U);
}
#endif
} /* namespace */

/**
 * \file            st_usb_host_stack_test.cpp
 * \brief           Official Host core uses existing packet ownership and power
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "usb_host_stack.h"
#include "usbh_ctlreq.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class StackTransport {
  public:
    MOCK_METHOD(nx_result_t, State, (nx_usb_host_state_t*));
    MOCK_METHOD(nx_result_t, Stop, ());
    MOCK_METHOD(nx_result_t, Vbus, (bool));
    MOCK_METHOD(nx_result_t, Open, (uint8_t, const nx_usb_host_pipe_t*));
    MOCK_METHOD(nx_result_t, Submit, (uint8_t, nx_usb_host_request_t*));
    MOCK_METHOD(nx_result_t, Service, ());
    MOCK_METHOD(nx_result_t, Restart, (nx_time_us_t));
    MOCK_METHOD(nx_result_t, Requirements,
                (uint8_t, size_t, nx_usb_host_storage_requirements_t*),
                (const));
    static nx_result_t Storage(const void* p, uint8_t pipe, size_t length,
                               nx_usb_host_storage_requirements_t* out) {
        return static_cast<const StackTransport*>(p)->Requirements(pipe, length,
                                                                   out);
    }
    static nx_result_t StartAgain(void* p, nx_time_us_t deadline) {
        return static_cast<StackTransport*>(p)->Restart(deadline);
    }
    static nx_result_t OpenPipe(void* p, uint8_t pipe,
                                const nx_usb_host_pipe_t* selected) {
        return static_cast<StackTransport*>(p)->Open(pipe, selected);
    }
    static nx_result_t SubmitPacket(void* p, uint8_t pipe,
                                    nx_usb_host_request_t* request) {
        return static_cast<StackTransport*>(p)->Submit(pipe, request);
    }
    static nx_result_t Poll(void* p) {
        return static_cast<StackTransport*>(p)->Service();
    }
    static nx_result_t Read(void* p, nx_usb_host_state_t* out) {
        return static_cast<StackTransport*>(p)->State(out);
    }
    static nx_result_t Close(void* p) {
        return static_cast<StackTransport*>(p)->Stop();
    }
    static nx_result_t Power(void* p, bool on) {
        return static_cast<StackTransport*>(p)->Vbus(on);
    }
};
static const nx_usb_host_ops_t stack_ops = {StackTransport::OpenPipe,
                                            nullptr,
                                            StackTransport::SubmitPacket,
                                            nullptr,
                                            StackTransport::Poll,
                                            StackTransport::Read,
                                            nullptr,
                                            nullptr,
                                            StackTransport::Close,
                                            StackTransport::Storage};

class StUsbHostStack : public testing::Test {
  protected:
    StrictMock<StackTransport> transport;
    const nx_usb_host_port_t port = {&stack_ops, &transport};
    nx_st_usb_host_stack_t stack = {};
    nx_st_usb_host_slot_t slots[2] = {};
    alignas(4) uint8_t staging[2][1024] = {};
    nx_st_usb_host_stack_config_t config = {
        &port,      slots, 2U,      StackTransport::Power,
        &transport, 1000U, 100000U, StackTransport::StartAgain,
        &transport};
    void SetUp() override {
        ASSERT_EQ(nx_native_clock_configure(true, 100U), NX_SUCCESS);
        for (size_t i = 0U; i < 2U; ++i) {
            slots[i].storage = staging[i];
            slots[i].capacity = sizeof(staging[i]);
        }
        EXPECT_CALL(transport, Requirements(_, _, _))
            .Times(testing::AnyNumber())
            .WillRepeatedly(Invoke([this](uint8_t pipe, size_t length,
                                          auto* out) {
                const size_t bytes = (slots[pipe].pipe.endpoint_address & 0x80U)
                                         ? slots[pipe].pipe.max_packet
                                         : length;
                *out = {(bytes + 3U) & ~size_t{3U}, 4U};
                return NX_SUCCESS;
            }));
    }
    void Start() {
        EXPECT_CALL(transport, State(_)).WillOnce(Invoke([](auto* out) {
            *out = nx_usb_host_state_t{};
            out->started = true;
            return NX_SUCCESS;
        }));
        EXPECT_CALL(transport, Vbus(true)).WillOnce(Return(NX_SUCCESS));
        ASSERT_EQ(
            nx_st_usb_host_stack_initialize(&stack, &config, nullptr, nullptr),
            NX_SUCCESS);
    }
    void Step() {
        EXPECT_CALL(transport, Service()).WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(transport, State(_)).WillOnce(Invoke([](auto* out) {
            *out = nx_usb_host_state_t{};
            out->started = true;
            return NX_SUCCESS;
        }));
        EXPECT_EQ(nx_st_usb_host_stack_step(&stack), NX_SUCCESS);
    }
    void Open(bool in) {
        EXPECT_CALL(transport, Open(0U, _))
            .WillOnce(Invoke([in](uint8_t, const nx_usb_host_pipe_t* pipe) {
                EXPECT_EQ(pipe->endpoint_address, in ? 0x81U : 1U);
                EXPECT_EQ(pipe->speed, NX_USB_HOST_FULL_SPEED);
                EXPECT_EQ(pipe->max_packet, 64U);
                return NX_SUCCESS;
            }));
        ASSERT_EQ(USBH_LL_OpenPipe(&stack.middleware, 0U, in ? 0x81U : 1U, 3U,
                                   USBH_SPEED_FULL, USB_EP_TYPE_BULK, 64U),
                  USBH_OK);
    }
    void TearDown() override {
        if (stack.config == nullptr)
            return;
        if (!stack.stopped) {
            EXPECT_CALL(transport, Stop()).WillOnce(Invoke([this]() {
                for (auto& slot : slots) {
                    if (slot.busy && nx_request_state(&slot.request.base) !=
                                         NX_REQUEST_SETTLED)
                        nx_request_settle(&slot.request.base,
                                          NX_ERROR_CANCELLED, 0U);
                }
                return NX_SUCCESS;
            }));
        } else {
            EXPECT_CALL(transport, Stop()).Times(0);
        }
        EXPECT_CALL(transport, Vbus(false)).WillOnce(Return(NX_SUCCESS));
        EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    }
};

TEST_F(StUsbHostStack,
       QueriedStorageControlsPaddingWithoutFixedWordAssumptions) {
    slots[0].storage = staging[0] + 1U;
    slots[0].capacity = 127U;
    Start();
    Open(false);
    uint8_t bytes[] = {1U, 2U, 3U};
    std::memset(slots[0].storage, 0x5a, slots[0].capacity);
    EXPECT_CALL(transport, Requirements(0U, 3U, _))
        .WillOnce(Invoke([](uint8_t, size_t, auto* out) {
            *out = {7U, 1U};
            return NX_SUCCESS;
        }));
    EXPECT_CALL(transport, Submit(0U, &slots[0].request))
        .WillOnce(Invoke([this, &bytes](uint8_t, auto* request) {
            EXPECT_EQ(0, std::memcmp(request->bytes, bytes, sizeof(bytes)));
            for (size_t i = 3U; i < 7U; ++i)
                EXPECT_EQ(0U, request->bytes[i]);
            EXPECT_EQ(0x5aU, request->bytes[7]);
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(USBH_OK, USBH_LL_SubmitURB(&stack.middleware, 0U, 0U,
                                         USB_EP_TYPE_BULK, 1U, bytes, 3U, 0U));
    nx_request_settle(&slots[0].request.base, NX_SUCCESS, 3U);
}

TEST_F(StUsbHostStack,
       QueriedStorageOverBudgetRejectsBeforeStagingOrAdmission) {
    Start();
    Open(false);
    uint8_t bytes[] = {1U, 2U, 3U};
    std::memset(staging[0], 0x5a, sizeof(staging[0]));
    EXPECT_CALL(transport, Requirements(0U, 3U, _))
        .WillOnce(Invoke([](uint8_t, size_t, auto* out) {
            *out = {2048U, 1U};
            return NX_SUCCESS;
        }));
    EXPECT_EQ(USBH_FAIL,
              USBH_LL_SubmitURB(&stack.middleware, 0U, 0U, USB_EP_TYPE_BULK, 1U,
                                bytes, 3U, 0U));
    EXPECT_EQ(0x5aU, staging[0][0]);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&slots[0].request.base));
}

TEST_F(StUsbHostStack, InvalidStagingRejectsBeforeTransportOrVbusEffects) {
    slots[1].storage = slots[0].storage;
    EXPECT_EQ(
        nx_st_usb_host_stack_initialize(&stack, &config, nullptr, nullptr),
        NX_ERROR_INVALID);
    EXPECT_EQ(stack.config, nullptr);
}

TEST_F(StUsbHostStack, OfficialCoreUsesCallerStateAndStopDrainsBeforeVbusOff) {
    Start();
    EXPECT_EQ(stack.middleware.pData, &stack);
    EXPECT_EQ(stack.middleware.gState, HOST_IDLE);
    EXPECT_EQ(stack.middleware.ClassNumber, 0U);
    EXPECT_EQ(stack.config, &config);
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_ERROR_BUSY);
    EXPECT_EQ(stack.config, &config);
    {
        testing::InSequence order;
        EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_SUCCESS));
        EXPECT_CALL(transport, Vbus(false)).WillOnce(Return(NX_SUCCESS));
    }
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_EQ(stack.config, nullptr);
}

TEST_F(StUsbHostStack, OutAggregatesPacketsAndOwnsSourceUntilSettlement) {
    Start();
    Open(false);
    uint8_t bytes[96];
    for (size_t i = 0U; i < sizeof(bytes); ++i)
        bytes[i] = static_cast<uint8_t>(i);
    EXPECT_CALL(transport, Submit(0U, &slots[0].request))
        .WillOnce(Invoke([this, &bytes](uint8_t, auto* request) {
            EXPECT_EQ(request->length, 64U);
            EXPECT_FALSE(request->data1);
            EXPECT_EQ(std::memcmp(request->bytes, bytes, 64U), 0);
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(USBH_LL_SubmitURB(&stack.middleware, 0U, 0U, USB_EP_TYPE_BULK, 1U,
                                bytes, sizeof(bytes), 0U),
              USBH_OK);
    EXPECT_EQ(USBH_LL_GetURBState(&stack.middleware, 0U), USBH_URB_IDLE);
    EXPECT_EQ(slots[0].bytes, bytes);
    EXPECT_EQ(USBH_LL_ClosePipe(&stack.middleware, 0U), USBH_BUSY);
    nx_request_settle(&slots[0].request.base, NX_SUCCESS, 64U);
    EXPECT_CALL(transport, Submit(0U, &slots[0].request))
        .WillOnce(Invoke([this, &bytes](uint8_t, auto* request) {
            EXPECT_EQ(request->length, 32U);
            EXPECT_TRUE(request->data1);
            EXPECT_EQ(std::memcmp(request->bytes, bytes + 64U, 32U), 0);
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    Step();
    EXPECT_EQ(USBH_LL_GetLastXferSize(&stack.middleware, 0U), 64U);
    EXPECT_EQ(slots[0].bytes, bytes);
    nx_request_settle(&slots[0].request.base, NX_SUCCESS, 32U);
    Step();
    EXPECT_EQ(USBH_LL_GetURBState(&stack.middleware, 0U), USBH_URB_DONE);
    EXPECT_EQ(USBH_LL_GetLastXferSize(&stack.middleware, 0U), 96U);
    EXPECT_EQ(USBH_LL_GetToggle(&stack.middleware, 0U), 0U);
    EXPECT_EQ(slots[0].bytes, nullptr);
}

TEST_F(StUsbHostStack, ShortInCopiesOnlyActualBytesAfterAcquireSettlement) {
    Start();
    Open(true);
    uint8_t bytes[96];
    std::memset(bytes, 0xEE, sizeof(bytes));
    EXPECT_CALL(transport, Submit(0U, &slots[0].request))
        .WillOnce(Invoke([](uint8_t, auto* request) {
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(USBH_LL_SubmitURB(&stack.middleware, 0U, 1U, USB_EP_TYPE_BULK, 1U,
                                bytes, sizeof(bytes), 0U),
              USBH_OK);
    std::memset(staging[0], 0x5A, 20U);
    EXPECT_EQ(bytes[0], 0xEEU);
    nx_request_settle(&slots[0].request.base, NX_SUCCESS, 20U);
    Step();
    EXPECT_EQ(bytes[0], 0x5AU);
    EXPECT_EQ(bytes[19], 0x5AU);
    EXPECT_EQ(bytes[20], 0xEEU);
    EXPECT_EQ(USBH_LL_GetURBState(&stack.middleware, 0U), USBH_URB_DONE);
    EXPECT_EQ(USBH_LL_GetLastXferSize(&stack.middleware, 0U), 20U);
    EXPECT_EQ(slots[0].bytes, nullptr);
}

TEST_F(StUsbHostStack, NakEndsTheUrbForExplicitOfficialCoreRetry) {
    Start();
    Open(true);
    uint8_t bytes[64] = {};
    EXPECT_CALL(transport, Submit(0U, &slots[0].request))
        .WillOnce(Invoke([](uint8_t, auto* request) {
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    ASSERT_EQ(USBH_LL_SubmitURB(&stack.middleware, 0U, 1U, USB_EP_TYPE_BULK, 1U,
                                bytes, sizeof(bytes), 0U),
              USBH_OK);
    nx_request_settle(&slots[0].request.base, NX_ERROR_NACK, 0U);
    Step();
    EXPECT_EQ(USBH_LL_GetURBState(&stack.middleware, 0U), USBH_URB_NOTREADY);
    EXPECT_EQ(slots[0].bytes, nullptr);
    EXPECT_FALSE(slots[0].busy);
    EXPECT_EQ(USBH_LL_GetToggle(&stack.middleware, 0U), 0U);
}

TEST_F(StUsbHostStack, RealDescriptorCoreCanUseItsOwnSetupAndDataBuffers) {
    Start();
    auto& host = stack.middleware;
    host.Control.pipe_out = 0U;
    host.Control.pipe_in = 1U;
    EXPECT_CALL(transport, Open(_, _))
        .Times(2)
        .WillRepeatedly(Return(NX_SUCCESS));
    ASSERT_EQ(USBH_LL_OpenPipe(&host, 0U, 0U, 0U, USBH_SPEED_FULL,
                               USB_EP_TYPE_CTRL, 8U),
              USBH_OK);
    ASSERT_EQ(USBH_LL_OpenPipe(&host, 1U, 0x80U, 0U, USBH_SPEED_FULL,
                               USB_EP_TYPE_CTRL, 8U),
              USBH_OK);
    EXPECT_EQ(USBH_Get_DevDesc(&host, 8U), USBH_BUSY);
    EXPECT_CALL(transport, Submit(0U, &slots[0].request))
        .WillOnce(Invoke([](uint8_t, auto* request) {
            const uint8_t setup[] = {0x80U, 6U, 0U, 1U, 0U, 0U, 8U, 0U};
            EXPECT_TRUE(request->setup);
            EXPECT_EQ(request->length, 8U);
            EXPECT_EQ(std::memcmp(request->bytes, setup, sizeof(setup)), 0);
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    EXPECT_EQ(USBH_Get_DevDesc(&host, 8U), USBH_BUSY);
    ASSERT_TRUE(slots[0].busy);
    nx_request_settle(&slots[0].request.base, NX_SUCCESS, 8U);
    Step();
    EXPECT_EQ(USBH_Get_DevDesc(&host, 8U), USBH_BUSY);
    EXPECT_CALL(transport, Submit(1U, &slots[1].request))
        .WillOnce(Invoke([](uint8_t, auto* request) {
            EXPECT_FALSE(request->setup);
            EXPECT_TRUE(request->data1);
            EXPECT_EQ(request->length, 8U);
            return nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        }));
    EXPECT_EQ(USBH_Get_DevDesc(&host, 8U), USBH_BUSY);
    ASSERT_TRUE(slots[1].busy);
    const uint8_t descriptor[] = {18U, 1U, 0U, 2U, 0U, 0U, 0U, 64U};
    std::memcpy(staging[1], descriptor, sizeof(descriptor));
    nx_request_settle(&slots[1].request.base, NX_SUCCESS, sizeof(descriptor));
    Step();
    EXPECT_EQ(std::memcmp(host.device.Data, descriptor, sizeof(descriptor)), 0);
}

TEST_F(StUsbHostStack, ExplicitBackendRestartsTheSameFullyDrainedController) {
    Start();
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(USBH_LL_Stop(&stack.middleware), USBH_OK);
    EXPECT_TRUE(stack.stopped);
    EXPECT_CALL(transport, Restart(_))
        .WillOnce(Invoke([](nx_time_us_t deadline) {
            EXPECT_GT(deadline, nx_time_now_us());
            EXPECT_NE(deadline, NX_DEADLINE_NEVER);
            return NX_SUCCESS;
        }));
    EXPECT_EQ(USBH_LL_Start(&stack.middleware), USBH_OK);
    EXPECT_FALSE(stack.stopped);
}

TEST_F(StUsbHostStack, PhysicalDisconnectCannotResetCoreBeforeTransportDrain) {
    Start();
    stack.connected = true;
    stack.middleware.device.is_connected = 1U;
    stack.middleware.gState = HOST_ABORT_STATE;
    EXPECT_CALL(transport, Service()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(transport, State(_)).WillOnce(Invoke([](auto* out) {
        *out = nx_usb_host_state_t{};
        out->started = true;
        return NX_SUCCESS;
    }));
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_st_usb_host_stack_step(&stack), NX_ERROR_BUSY);
    EXPECT_TRUE(stack.connected);
    EXPECT_EQ(stack.middleware.device.is_connected, 1U);
    EXPECT_EQ(stack.middleware.gState, HOST_ABORT_STATE);
}

TEST_F(StUsbHostStack, DrainedDisconnectLetsActualCoreRequestExplicitRestart) {
    Start();
    stack.connected = true;
    stack.middleware.device.is_connected = 1U;
    stack.middleware.gState = HOST_ABORT_STATE;
    EXPECT_CALL(transport, Service()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(transport, State(_)).WillOnce(Invoke([](auto* out) {
        *out = nx_usb_host_state_t{};
        return NX_SUCCESS;
    }));
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(transport, Restart(_)).WillOnce(Invoke([](auto deadline) {
        EXPECT_GT(deadline, nx_time_now_us());
        return NX_SUCCESS;
    }));
    EXPECT_EQ(nx_st_usb_host_stack_step(&stack), NX_SUCCESS);
    EXPECT_FALSE(stack.connected);
    EXPECT_FALSE(stack.stopped);
    EXPECT_EQ(stack.middleware.device.is_connected, 0U);
    EXPECT_EQ(stack.middleware.gState, HOST_IDLE);
}

TEST_F(StUsbHostStack,
       PendingPowerWithdrawalDoesNotStopTheRetiredTransportAgain) {
    Start();
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(transport, Vbus(false)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_ERROR_BUSY);
    EXPECT_TRUE(stack.stopped);
    testing::Mock::VerifyAndClearExpectations(&transport);
    EXPECT_CALL(transport, Stop()).Times(0);
    EXPECT_CALL(transport, Vbus(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_EQ(nullptr, stack.config);
}

TEST_F(StUsbHostStack, ClassPipeCloseAfterStopUsesOnlyRetiredSoftwareSlots) {
    Start();
    Open(false);
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_SUCCESS));
    ASSERT_EQ(USBH_LL_Stop(&stack.middleware), USBH_OK);
    ASSERT_TRUE(stack.stopped);
    EXPECT_EQ(USBH_LL_ClosePipe(&stack.middleware, 0U), USBH_OK);
    EXPECT_FALSE(slots[0].configured);
}

TEST_F(StUsbHostStack, ClassDeinitRunsOnlyAfterDrainAndCompletesOnce) {
    Start();
    static unsigned deinitialized = 0U;
    deinitialized = 0U;
    stack.selected_class.DeInit = [](USBH_HandleTypeDef*) {
        ++deinitialized;
        return USBH_OK;
    };
    stack.middleware.pActiveClass = &stack.selected_class;
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_ERROR_BUSY);
    EXPECT_EQ(deinitialized, 0U);
    EXPECT_CALL(transport, Stop()).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(transport, Vbus(false)).WillOnce(Return(NX_SUCCESS));
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_EQ(deinitialized, 1U);
    EXPECT_EQ(nx_st_usb_host_stack_close(&stack), NX_SUCCESS);
    EXPECT_EQ(deinitialized, 1U);
}

TEST_F(StUsbHostStack, MissingRequiredClassCallbackRejectsBeforeAnyEffect) {
    ON_CALL(transport, State(_)).WillByDefault(Invoke([](auto* out) {
        *out = nx_usb_host_state_t{};
        out->started = true;
        return NX_SUCCESS;
    }));
    for (unsigned missing = 0U; missing < 4U; ++missing) {
        USBH_ClassTypeDef record{};
        record.Init = record.DeInit = record.Requests =
            record.BgndProcess = [](USBH_HandleTypeDef*) { return USBH_OK; };
        if (missing == 0U)
            record.Init = nullptr;
        else if (missing == 1U)
            record.DeInit = nullptr;
        else if (missing == 2U)
            record.Requests = nullptr;
        else
            record.BgndProcess = nullptr;
        EXPECT_EQ(
            nx_st_usb_host_stack_initialize(&stack, &config, &record, nullptr),
            NX_ERROR_INVALID);
        EXPECT_EQ(stack.config, nullptr);
        if (stack.config != nullptr)
            return;
    }
}

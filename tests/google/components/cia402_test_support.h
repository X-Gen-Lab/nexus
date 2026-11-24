/**
 * \file            cia402_test_support.h
 * \brief           External classic-CAN transport fixture for drive contracts
 * \author          Nexus Team
 */
#ifndef NEXUS_CIA402_TEST_SUPPORT_H
#define NEXUS_CIA402_TEST_SUPPORT_H
#include "nexus/components/canopen.h"
#include <algorithm>
#include <deque>
#include <vector>
namespace nexus_drive_test {
class DriveBus {
  public:
    nx_time_us_t now{1000U};
    std::deque<nx_can_event_t> rx;
    std::vector<nx_can_frame_t> sent;
    nx_can_tx_request_t* pending{nullptr};
    bool finish{true};
    static nx_time_us_t clock(void* context) {
        return static_cast<DriveBus*>(context)->now;
    }
    static nx_result_t submit(void* context, nx_can_tx_request_t* request) {
        auto& bus = *static_cast<DriveBus*>(context);
        if (bus.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        const auto result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            bus.pending = request;
            bus.sent.push_back(request->frame);
        }
        return result;
    }
    static nx_result_t cancel(void* context, nx_can_tx_request_t* request) {
        auto& bus = *static_cast<DriveBus*>(context);
        if (bus.pending != request) {
            return NX_ERROR_STATE;
        }
        bus.pending = nullptr;
        nx_request_settle(&request->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    }
    static nx_result_t service(void* context) {
        auto& bus = *static_cast<DriveBus*>(context);
        if (bus.pending != nullptr && bus.finish) {
            auto* request = bus.pending;
            bus.pending = nullptr;
            nx_request_settle(&request->base, NX_SUCCESS, request->frame.dlc);
        }
        return NX_SUCCESS;
    }
    static nx_result_t read(void* context, nx_can_event_t* out, size_t capacity,
                            size_t* count) {
        auto& bus = *static_cast<DriveBus*>(context);
        *count = 0U;
        if (capacity != 0U && !bus.rx.empty()) {
            *out = bus.rx.front();
            bus.rx.pop_front();
            *count = 1U;
        }
        return NX_SUCCESS;
    }
    static nx_result_t state(void*, nx_can_state_t* out) {
        *out = {};
        out->started = true;
        return NX_SUCCESS;
    }
    void push(uint32_t id, std::initializer_list<uint8_t> bytes) {
        nx_can_event_t event{};
        event.kind = NX_CAN_EVENT_FRAME;
        event.frame = {id,
                       NX_CAN_STANDARD,
                       NX_CAN_DATA,
                       static_cast<uint8_t>(bytes.size()),
                       {}};
        std::copy(bytes.begin(), bytes.end(), event.frame.data);
        rx.push_back(event);
    }
    const nx_can_frame_t* last(uint32_t id) const {
        for (auto frame = sent.rbegin(); frame != sent.rend(); ++frame) {
            if (frame->id == id) {
                return &*frame;
            }
        }
        return nullptr;
    }
    nx_can_ops_t ops{submit, cancel,  service, read,
                     state,  nullptr, nullptr, nullptr};
    nx_can_port_t port{&ops, this};
};

} /* namespace nexus_drive_test */
#endif

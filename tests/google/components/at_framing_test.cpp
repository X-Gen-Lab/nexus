/**
 * \file            at_framing_test.cpp
 * \brief           Optional non-line AT framing preserves same-read payloads
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/at_client.h"
#include <algorithm>
#include <array>
#include <deque>
#include <gtest/gtest.h>
#include <string>
#include <vector>
namespace {
struct AtFraming {
    nx_at_client_t client{};
    std::array<uint8_t, 64> line{};
    std::array<uint8_t, 128> transmit{};
    std::deque<uint8_t> input;
    std::vector<nx_at_event_kind_t> events;
    std::vector<std::string> views;
    std::vector<uint8_t> binary;
    nx_time_us_t now{10U};
    size_t fragment{64U};
    bool accept_header{true};
    static nx_result_t write(void*, const uint8_t*, size_t bytes,
                             size_t* accepted, nx_time_us_t) {
        *accepted = bytes;
        return NX_SUCCESS;
    }
    static nx_result_t read(void* context, uint8_t* data, size_t bytes,
                            size_t* received, nx_time_us_t) {
        auto& self = *static_cast<AtFraming*>(context);
        *received = std::min({bytes, self.input.size(), self.fragment});
        for (size_t i = 0U; i < *received; ++i) {
            data[i] = self.input.front();
            self.input.pop_front();
        }
        return *received == 0U ? NX_ERROR_BUSY : NX_SUCCESS;
    }
    static nx_time_us_t clock(void* context) {
        return static_cast<AtFraming*>(context)->now;
    }
    static void event(void* context, const nx_at_event_t* event) {
        auto& self = *static_cast<AtFraming*>(context);
        self.events.push_back(event->kind);
        self.views.emplace_back(reinterpret_cast<const char*>(event->data),
                                event->size);
        if (event->kind == NX_AT_HEADER && self.accept_header) {
            EXPECT_EQ(self.views.back(), "+IPD,5");
            EXPECT_EQ(nx_at_expect_binary(&self.client, 5U), NX_SUCCESS);
        }
        if (event->kind == NX_AT_BINARY) {
            self.binary.insert(self.binary.end(), event->data,
                               event->data + event->size);
        }
    }
    void open(bool selected = true) {
        static const nx_byte_stream_ops_t ops{write, read};
        const nx_at_config_t config{{&ops, this},    {clock, this},
                                    event,           this,
                                    nullptr,         0U,
                                    line.data(),     line.size(),
                                    transmit.data(), transmit.size()};
        ASSERT_EQ(nx_at_init(&client, &config), NX_SUCCESS);
        if (selected) {
            static const nx_at_framing_t framing{"+IPD,", ':', '>'};
            ASSERT_EQ(nx_at_set_framing(&client, &framing), NX_SUCCESS);
        }
    }
    void feed(const std::string& text) {
        input.insert(input.end(), text.begin(), text.end());
    }
    void drain() {
        for (unsigned count = 0U; count < 256U && !input.empty(); ++count) {
            EXPECT_EQ(nx_at_service(&client, 100U, 2U), NX_SUCCESS);
        }
        EXPECT_TRUE(input.empty());
    }
};

TEST(AtFraming, PromptWithoutNewlineFollowsTheCommandCompletion) {
    AtFraming h;
    h.open();
    const uint8_t command[]{'A', 'T'};
    ASSERT_EQ(nx_at_submit(&h.client, command, sizeof(command), 100U),
              NX_SUCCESS);
    h.feed("OK\r\n>");
    h.drain();
    EXPECT_EQ(h.events,
              (std::vector<nx_at_event_kind_t>{NX_AT_COMPLETE, NX_AT_PROMPT}));
    EXPECT_EQ(h.views.back(), ">");
}

TEST(AtFraming, HeaderAndBinaryInSameReadPreserveAllControlBytes) {
    AtFraming h;
    h.open();
    h.feed("+IPD,5:");
    const std::array<uint8_t, 5> payload{'a', '\r', '\n', 0U, '>'};
    h.input.insert(h.input.end(), payload.begin(), payload.end());
    h.feed("\r\nOK\r\n");
    h.drain();
    EXPECT_EQ(h.binary, (std::vector<uint8_t>(payload.begin(), payload.end())));
    EXPECT_EQ(h.events.front(), NX_AT_HEADER);
    EXPECT_EQ(std::count(h.events.begin(), h.events.end(), NX_AT_PROMPT), 0);
    EXPECT_EQ(h.events.back(), NX_AT_RESPONSE);
    EXPECT_EQ(h.views.back(), "OK");
}

TEST(AtFraming, FragmentedHeaderAndPayloadKeepTheirExactBoundaries) {
    AtFraming h;
    h.open();
    h.fragment = 1U;
    h.feed("+IPD,5:hello\r\n+IPDx,5:text\r\n");
    h.drain();
    EXPECT_EQ(h.binary, (std::vector<uint8_t>{'h', 'e', 'l', 'l', 'o'}));
    EXPECT_EQ(std::count(h.events.begin(), h.events.end(), NX_AT_HEADER), 1);
    EXPECT_EQ(h.views.back(), "+IPDx,5:text");
}

TEST(AtFraming, RejectedHeaderLatchesDesynchronizationBeforePayload) {
    AtFraming h;
    h.open();
    h.accept_header = false;
    h.feed("+IPD,bad:payload\r\n");
    EXPECT_EQ(nx_at_service(&h.client, 100U, 1U), NX_ERROR_STATE);
    EXPECT_TRUE(h.binary.empty());
    EXPECT_EQ(nx_at_service(&h.client, 100U, 1U), NX_ERROR_STATE);
}

TEST(AtFraming, DescriptorChangesRequireAQuiescentParserAndValidTokens) {
    AtFraming h;
    h.open(false);
    const nx_at_framing_t invalid{"+IPD,", '\n', '>'};
    EXPECT_EQ(nx_at_set_framing(&h.client, &invalid), NX_ERROR_INVALID);
    const nx_at_framing_t valid{"+IPD,", ':', '>'};
    const uint8_t command[]{'A', 'T'};
    ASSERT_EQ(nx_at_submit(&h.client, command, sizeof(command), 100U),
              NX_SUCCESS);
    EXPECT_EQ(nx_at_set_framing(&h.client, &valid), NX_ERROR_BUSY);
}

TEST(AtFraming, UnselectedParserKeepsItsOrdinaryLineSemantics) {
    AtFraming h;
    h.open(false);
    h.feed(">\r\n+IPD,5:hello\r\n");
    h.drain();
    EXPECT_EQ(h.events, (std::vector<nx_at_event_kind_t>{NX_AT_RESPONSE,
                                                         NX_AT_RESPONSE}));
    EXPECT_TRUE(h.binary.empty());
}
} /* namespace */

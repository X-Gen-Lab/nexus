/**
 * \file            common_storage_model.h
 * \brief           External NOR media and clock boundaries for storage tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_TEST_COMMON_STORAGE_MODEL_H
#define NEXUS_TEST_COMMON_STORAGE_MODEL_H

#include "nexus/io/flash.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace common_storage {
/** \brief Model the medium only; all storage algorithms remain production C. */
class Flash {
  public:
    std::vector<uint8_t> bytes;
    nx_flash_erase_span_t span{0U, 4096U, 4U};
    nx_flash_geometry_t geometry{0U, 16384U, 4U, &span, 1U, 0U};
    nx_time_us_t now{10U};
    nx_time_us_t after_read_time{NX_DEADLINE_NEVER};
    nx_result_t write_result{NX_SUCCESS};
    nx_result_t erase_result{NX_SUCCESS};
    nx_result_t recover_result{NX_SUCCESS};
    unsigned reads{0U};
    unsigned programs{0U};
    unsigned erases{0U};
    unsigned recoveries{0U};
    nx_time_us_t last_deadline{0U};
    size_t fail_after{SIZE_MAX};
    Flash() : bytes(16384U, 0xffU) {
    }
    static nx_time_us_t time(void* context) {
        return static_cast<Flash*>(context)->now;
    }
    nx_clock_t clock() {
        return {time, this};
    }
    static const nx_flash_geometry_t* describe(const void* context) {
        return &static_cast<const Flash*>(context)->geometry;
    }
    static nx_result_t read(const void* context, uint32_t offset, void* data,
                            size_t length) {
        auto& self = *const_cast<Flash*>(static_cast<const Flash*>(context));
        ++self.reads;
        if (offset > self.bytes.size() || length > self.bytes.size() - offset) {
            return NX_ERROR_INVALID;
        }
        std::memcpy(data, self.bytes.data() + offset, length);
        if (self.after_read_time != NX_DEADLINE_NEVER) {
            self.now = self.after_read_time;
        }
        return NX_SUCCESS;
    }
    static nx_result_t program(void* context, uint32_t offset, const void* data,
                               size_t length, nx_time_us_t deadline) {
        auto& self = *static_cast<Flash*>(context);
        ++self.programs;
        self.last_deadline = deadline;
        if (self.programs > self.fail_after) {
            return NX_ERROR_IO;
        }
        const auto* source = static_cast<const uint8_t*>(data);
        for (size_t i = 0U; i < length; ++i) {
            if ((self.bytes[offset + i] & source[i]) != source[i]) {
                return NX_ERROR_IO;
            }
            self.bytes[offset + i] &= source[i];
        }
        return self.write_result;
    }
    static nx_result_t erase(void* context, uint32_t offset, size_t length,
                             nx_time_us_t deadline) {
        auto& self = *static_cast<Flash*>(context);
        ++self.erases;
        self.last_deadline = deadline;
        std::fill_n(self.bytes.begin() + offset, length, 0xffU);
        return self.erase_result;
    }
    static nx_result_t recover(void* context) {
        auto& self = *static_cast<Flash*>(context);
        ++self.recoveries;
        return self.recover_result;
    }
    static const nx_flash_ops_t ops;
    nx_flash_port_t port{&ops, this};
    nx_flash_region_t region() {
        return {&port, 0U, geometry.size, true};
    }
};
inline const nx_flash_ops_t Flash::ops{describe, read, program, erase, recover};
} /* namespace common_storage */
#endif

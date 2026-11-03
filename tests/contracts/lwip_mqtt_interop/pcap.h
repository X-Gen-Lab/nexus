/**
 * \file            pcap.h
 * \brief           Test-only bounded capture of successful Ethernet IPC sends
 * \author          Nexus Team
 */
#ifndef NEXUS_TESTS_LWIP_MQTT_INTEROP_PCAP_H
#define NEXUS_TESTS_LWIP_MQTT_INTEROP_PCAP_H

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nexus::test {
/**
 * \brief Caller-owned single-executor PCAP, with no retained frame spans.
 * \note Host C++17 fixture only. Open accepts one canonical private directory
 * and client.pcap or relay.pcap, without aliases or replacement. The clock is
 * called with nullptr and supplies already-relative host microseconds in the
 * same case domain, never physical wire timestamps. Record copies and writes
 * the successful IPC send before its request settles. Failure latches without
 * affecting network ownership. Finish joins file I/O; it is not network stop.
 */
class PcapCapture {
  public:
    PcapCapture() = default;
    PcapCapture(const PcapCapture&) = delete;
    PcapCapture& operator=(const PcapCapture&) = delete;
    ~PcapCapture() {
        if (descriptor_ >= 0) {
            (void)::close(descriptor_);
        }
    }

    bool Open(const char* directory, const char* filename,
              uint64_t (*clock)(void*)) noexcept {
        if (attempted_ || finished_) {
            return Fail();
        }
        attempted_ = true;
        if (directory == nullptr || filename == nullptr || clock == nullptr ||
            (std::strcmp(filename, "client.pcap") != 0 &&
             std::strcmp(filename, "relay.pcap") != 0)) {
            return Fail();
        }
        clock_ = clock;
        last_time_ = clock_(nullptr);
        if (last_time_ > maximum_time) {
            return Fail();
        }
        const int parent = OpenDirectory(directory);
        if (parent < 0) {
            return Fail();
        }
        descriptor_ = ::openat(
            parent, filename,
            O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        const bool closed = ::close(parent) == 0;
        if (descriptor_ < 0 || !closed) {
            return Fail();
        }
        struct stat facts {};
        if (::fstat(descriptor_, &facts) != 0 || !PrivateFile(facts)) {
            return Fail();
        }
        std::array<uint8_t, 24> header{};
        Put32(header.data(), UINT32_C(0xa1b2c3d4));
        header[4] = 2U;
        header[6] = 4U;
        Put32(header.data() + 16U, maximum_frame);
        Put32(header.data() + 20U, 1U);
        opened_ = Write(header.data(), header.size());
        return opened_;
    }

    static void Record(void* context, const uint8_t* bytes,
                       size_t size) noexcept {
        if (context != nullptr) {
            static_cast<PcapCapture*>(context)->RecordBytes(bytes, size);
        }
    }

    bool Finish() noexcept {
        if (finished_) {
            return opened_ && !failed_;
        }
        finished_ = true;
        if (descriptor_ < 0 || !opened_) {
            failed_ = true;
        }
        if (descriptor_ >= 0) {
            struct stat facts {};
            if (::fstat(descriptor_, &facts) != 0 || !PrivateFile(facts) ||
                facts.st_size < 0 ||
                static_cast<uint64_t>(facts.st_size) != bytes_ ||
                ::fsync(descriptor_) != 0) {
                failed_ = true;
            }
            if (::close(descriptor_) != 0) {
                failed_ = true;
            }
            descriptor_ = -1;
        }
        return opened_ && !failed_;
    }

    size_t Frames() const noexcept {
        return frames_;
    }
    size_t Bytes() const noexcept {
        return bytes_;
    }

  private:
    static constexpr uint32_t maximum_frame = 1514U;
    static constexpr size_t maximum_frames = 1024U;
    static constexpr size_t maximum_bytes = 1048576U;
    static constexpr uint64_t maximum_time = UINT64_C(20000000);
    int descriptor_ = -1;
    uint64_t (*clock_)(void*) = nullptr;
    uint64_t last_time_ = 0U;
    size_t frames_ = 0U;
    size_t bytes_ = 0U;
    bool attempted_ = false;
    bool opened_ = false;
    bool failed_ = false;
    bool finished_ = false;

    bool Fail() noexcept {
        failed_ = true;
        return false;
    }
    static void Put32(uint8_t* bytes, uint32_t value) noexcept {
        for (unsigned i = 0U; i < 4U; ++i) {
            bytes[i] = static_cast<uint8_t>(value >> (i * 8U));
        }
    }
    static bool PrivateFile(const struct stat& facts) noexcept {
        return S_ISREG(facts.st_mode) && facts.st_uid == ::geteuid() &&
               (facts.st_mode & 07777U) == 0600U && facts.st_nlink == 1U;
    }
    /** \brief Pin every directory component; never resolve a path alias. */
    static int OpenDirectory(const char* path) noexcept {
        const size_t length = ::strnlen(path, PATH_MAX);
        if (length < 2U || length >= PATH_MAX || path[0] != '/' ||
            path[length - 1U] == '/') {
            return -1;
        }
        int current =
            ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (current < 0) {
            return -1;
        }
        size_t offset = 1U;
        while (offset < length) {
            size_t end = offset;
            while (end < length && path[end] != '/') {
                ++end;
            }
            const size_t count = end - offset;
            if (count == 0U || count > NAME_MAX ||
                (count == 1U && path[offset] == '.') ||
                (count == 2U && path[offset] == '.' &&
                 path[offset + 1U] == '.')) {
                (void)::close(current);
                return -1;
            }
            std::array<char, NAME_MAX + 1U> component{};
            std::memcpy(component.data(), path + offset, count);
            const int next =
                ::openat(current, component.data(),
                         O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            const bool closed = ::close(current) == 0;
            if (next < 0 || !closed) {
                if (next >= 0) {
                    (void)::close(next);
                }
                return -1;
            }
            current = next;
            offset = end + 1U;
        }
        struct stat facts {};
        if (::fstat(current, &facts) != 0 || !S_ISDIR(facts.st_mode) ||
            facts.st_uid != ::geteuid() || (facts.st_mode & 07777U) != 0700U) {
            (void)::close(current);
            return -1;
        }
        return current;
    }
    /** \brief Bounded write attempts; a short or interrupted failure stays bad.
     */
    bool Write(const uint8_t* bytes, size_t count) noexcept {
        size_t offset = 0U;
        for (unsigned attempt = 0U; attempt < 4U && offset < count; ++attempt) {
            const ssize_t result =
                ::write(descriptor_, bytes + offset, count - offset);
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result <= 0 || static_cast<size_t>(result) > count - offset) {
                return Fail();
            }
            const size_t accepted = static_cast<size_t>(result);
            offset += accepted;
            bytes_ += accepted;
        }
        return offset == count || Fail();
    }
    void RecordBytes(const uint8_t* bytes, size_t size) noexcept {
        if (failed_) {
            return;
        }
        if (!opened_ || finished_ || bytes == nullptr || size < 14U ||
            size > maximum_frame || frames_ >= maximum_frames ||
            16U + size > maximum_bytes - bytes_) {
            (void)Fail();
            return;
        }
        const uint64_t now = clock_(nullptr);
        if (now < last_time_ || now > maximum_time) {
            (void)Fail();
            return;
        }
        last_time_ = now;
        std::array<uint8_t, maximum_frame + 16U> record{};
        Put32(record.data(), static_cast<uint32_t>(now / 1000000U));
        Put32(record.data() + 4U, static_cast<uint32_t>(now % 1000000U));
        Put32(record.data() + 8U, static_cast<uint32_t>(size));
        Put32(record.data() + 12U, static_cast<uint32_t>(size));
        std::memcpy(record.data() + 16U, bytes, size);
        if (Write(record.data(), size + 16U)) {
            ++frames_;
        }
    }
};
} /* namespace nexus::test */
#endif /* NEXUS_TESTS_LWIP_MQTT_INTEROP_PCAP_H */

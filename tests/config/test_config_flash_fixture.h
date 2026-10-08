#ifndef TEST_CONFIG_FLASH_FIXTURE_H
#define TEST_CONFIG_FLASH_FIXTURE_H
#include "nexus/file_flash.h"
#include "config/config_backend.h"
#include <cstdio>
#if !defined(_WIN32)
#include <unistd.h>
#endif

/* Every fixture owns a real persistent file; no implicit RAM-backed flash. */
class ConfigFlashModel {
  public:
    bool Bind() {
#if defined(_WIN32)
        return false; /* No validated Windows persistent Flash model. */
#else
        static unsigned counter = 0;
        std::snprintf(path_, sizeof(path_), "/tmp/nexus-config-%ld-%u.flash",
                      static_cast<long>(getpid()), ++counter);
        if (nx_file_flash_open(&flash_, path_, 65536, 256, 8) != NX_STORAGE_OK)
            return false;
        if (nx_storage_open(&storage_, &flash_.port, 0, 32768) != NX_STORAGE_OK)
            return false;
        return config_backend_flash_bind(&storage_) == CONFIG_OK;
#endif
    }
    void Release() {
        config_backend_flash_bind(nullptr);
#if !defined(_WIN32)
        nx_file_flash_close(&flash_);
        if (path_[0]) unlink(path_);
        path_[0] = 0;
#endif
    }
    ~ConfigFlashModel() { Release(); }
  private:
    char path_[128]{};
    nx_file_flash_t flash_{-1, {}, -1, 0};
    nx_storage_t storage_{};
};
#endif

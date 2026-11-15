/**
 * \file            mcuboot_boot_call.c
 * \brief           Keep upstream C11 static assertions outside C++ fixtures.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "mcuboot_boot_call.h"
#include "bootutil/bootutil.h"
#include "bootutil/image.h"

int nx_test_mcuboot_boot(nx_test_mcuboot_selected_t* selected) {
    struct boot_rsp response = {0};
    fih_ret result = boot_go(&response);
    if (FIH_NOT_EQ(result, FIH_SUCCESS) || response.br_hdr == NULL) {
        return -1;
    }
    *selected = (nx_test_mcuboot_selected_t){
        response.br_image_off, response.br_hdr->ih_hdr_size,
        response.br_hdr->ih_ver.iv_major, response.br_hdr->ih_ver.iv_minor};
    return 0;
}

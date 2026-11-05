/**
 * \file            main.c
 * \brief           Public-only C consumer, linked but never executed by CI
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "composition.h"

/** \brief No erase/program is issued without a product-owned scratch region. */
int main(void) {
    if (!nx_h743_w25q128_public_headers_hidden())
        return 1;
    nx_result_t prepared = nx_h743_w25q128_reference_prepare();
    nx_h743_w25q128_reference_service();
    const nx_mapped_storage_port_t* port = nx_h743_w25q128_reference_storage();
    if (port != NULL && nx_mapped_storage_geometry(port) == NULL)
        return 2;
    nx_result_t stopped = nx_h743_w25q128_reference_stop();
    return prepared == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 3;
}

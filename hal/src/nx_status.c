/**
 * \file            nx_status.c
 * \brief           Nexus platform unified status/error codes implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#include "hal/nx_status.h"

/*
 * Global error callback state
 */
static nx_error_callback_t g_error_callback = NULL;
static void* g_error_callback_user_data = NULL;

/**
 * \brief           Status code to string mapping table
 */
static const struct {
    nx_status_t status;
    const char* str;
} status_strings[] = {
    /* Success */
    {NX_OK, "OK"},

    /* General errors */
    {NX_ERR_GENERIC, "Generic error"},
    {NX_ERR_INVALID_PARAM, "Invalid parameter"},
    {NX_ERR_NULL_PTR, "Null pointer"},
    {NX_ERR_NOT_SUPPORTED, "Not supported"},
    {NX_ERR_NOT_FOUND, "Not found"},
    {NX_ERR_INVALID_SIZE, "Invalid size"},

    /* State errors */
    {NX_ERR_NOT_INIT, "Not initialized"},
    {NX_ERR_ALREADY_INIT, "Already initialized"},
    {NX_ERR_INVALID_STATE, "Invalid state"},
    {NX_ERR_BUSY, "Device busy"},
    {NX_ERR_SUSPENDED, "Device suspended"},

    /* Resource errors */
    {NX_ERR_NO_MEMORY, "Out of memory"},
    {NX_ERR_NO_RESOURCE, "Resource unavailable"},
    {NX_ERR_RESOURCE_BUSY, "Resource busy"},
    {NX_ERR_LOCKED, "Resource locked"},
    {NX_ERR_FULL, "Buffer full"},
    {NX_ERR_EMPTY, "Buffer empty"},

    /* Timeout errors */
    {NX_ERR_TIMEOUT, "Timeout"},
    {NX_ERR_WOULD_BLOCK, "Would block"},

    /* IO errors */
    {NX_ERR_IO, "IO error"},
    {NX_ERR_OVERRUN, "Buffer overrun"},
    {NX_ERR_UNDERRUN, "Buffer underrun"},
    {NX_ERR_PARITY, "Parity error"},
    {NX_ERR_FRAMING, "Framing error"},
    {NX_ERR_NOISE, "Noise error"},
    {NX_ERR_NACK, "NACK received"},
    {NX_ERR_BUS, "Bus error"},
    {NX_ERR_ARBITRATION, "Arbitration lost"},

    /* DMA errors */
    {NX_ERR_DMA, "DMA error"},
    {NX_ERR_DMA_TRANSFER, "DMA transfer error"},
    {NX_ERR_DMA_CONFIG, "DMA configuration error"},

    /* Data errors */
    {NX_ERR_NO_DATA, "No data available"},
    {NX_ERR_DATA_SIZE, "Data size error"},
    {NX_ERR_CRC, "CRC error"},
    {NX_ERR_CHECKSUM, "Checksum error"},

    /* Permission errors */
    {NX_ERR_PERMISSION, "Permission denied"},
    {NX_ERR_READ_ONLY, "Read-only resource"},
};

const char* nx_status_to_string(nx_status_t status) {
    size_t i;
    size_t count = sizeof(status_strings) / sizeof(status_strings[0]);

    for (i = 0; i < count; i++) {
        if (status_strings[i].status == status) {
            return status_strings[i].str;
        }
    }

    return "Unknown error";
}

void nx_set_error_callback(nx_error_callback_t callback, void* user_data) {
    g_error_callback = callback;
    g_error_callback_user_data = user_data;
}

void nx_report_error(nx_status_t status, const char* module, const char* msg) {
    if (g_error_callback != NULL && NX_IS_ERROR(status)) {
        g_error_callback(g_error_callback_user_data, status, module, msg);
    }
}

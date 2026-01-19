/**
 * \file            nx_adapter.c
 * \brief           Interface conversion adapter implementation
 * \author          Nexus Team
 */

#include "hal/base/nx_adapter.h"
#include "nexus_config.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* External dependencies                                                     */
/*---------------------------------------------------------------------------*/

/* OSAL yield function - weak definition for standalone testing */
NX_WEAK void osal_yield(void) {
    /* Default: no-op */
}

/* Tick function - weak definition for standalone testing */
NX_WEAK uint32_t nx_get_tick_ms(void) {
    static uint32_t tick = 0;
    return tick++;
}

/*---------------------------------------------------------------------------*/
/* Async to Sync TX Adapter                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async to sync TX adapter structure
 */
typedef struct {
    nx_tx_sync_t sync;    /**< Sync interface (must be first) */
    nx_tx_async_t* async; /**< Wrapped async interface */
} nx_tx_async_to_sync_adapter_t;

/**
 * \brief           Adapter send implementation
 */
static nx_status_t tx_async_to_sync_send(nx_tx_sync_t* self,
                                         const uint8_t* data, size_t len,
                                         uint32_t timeout_ms) {
    nx_tx_async_to_sync_adapter_t* adapter =
        NX_CONTAINER_OF(self, nx_tx_async_to_sync_adapter_t, sync);

    /* Start async send */
    nx_status_t status = adapter->async->send(adapter->async, data, len);
    if (status != NX_OK) {
        return status;
    }

    /* Poll until complete or timeout */
    uint32_t start = nx_get_tick_ms();
    while (adapter->async->get_state(adapter->async) == NX_ERR_BUSY) {
        if (nx_get_tick_ms() - start > timeout_ms) {
            return NX_ERR_TIMEOUT;
        }
        osal_yield();
    }

    return NX_OK;
}

/* Static adapter pool for async-to-sync TX */
static nx_tx_async_to_sync_adapter_t g_tx_a2s_adapters[4];
static uint8_t g_tx_a2s_used[4] = {0};

nx_tx_sync_t* nx_tx_async_to_sync(nx_tx_async_t* tx_async) {
    if (tx_async == NULL) {
        return NULL;
    }

    /* Find free adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_tx_a2s_adapters); i++) {
        if (!g_tx_a2s_used[i]) {
            g_tx_a2s_used[i] = 1;
            nx_tx_async_to_sync_adapter_t* adapter = &g_tx_a2s_adapters[i];

            adapter->async = tx_async;
            NX_INIT_TX_SYNC(&adapter->sync, tx_async_to_sync_send);

            return &adapter->sync;
        }
    }

    return NULL; /* No free slots */
}

void nx_tx_async_to_sync_release(nx_tx_sync_t* adapter) {
    if (adapter == NULL) {
        return;
    }

    /* Find and release adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_tx_a2s_adapters); i++) {
        if (&g_tx_a2s_adapters[i].sync == adapter) {
            g_tx_a2s_used[i] = 0;
            memset(&g_tx_a2s_adapters[i], 0, sizeof(g_tx_a2s_adapters[i]));
            return;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Async to Sync RX Adapter                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Async to sync RX adapter structure
 */
typedef struct {
    nx_rx_sync_t sync;    /**< Sync interface (must be first) */
    nx_rx_async_t* async; /**< Wrapped async interface */
} nx_rx_async_to_sync_adapter_t;

/**
 * \brief           Adapter receive implementation
 */
static nx_status_t rx_async_to_sync_receive(nx_rx_sync_t* self, uint8_t* data,
                                            size_t* len, uint32_t timeout_ms) {
    nx_rx_async_to_sync_adapter_t* adapter =
        NX_CONTAINER_OF(self, nx_rx_async_to_sync_adapter_t, sync);

    uint32_t start = nx_get_tick_ms();
    size_t buf_size = *len;

    /* Poll until data available or timeout */
    while (1) {
        *len = buf_size;
        nx_status_t status = adapter->async->receive(adapter->async, data, len);
        if (status == NX_OK) {
            return NX_OK;
        }

        if (nx_get_tick_ms() - start > timeout_ms) {
            *len = 0;
            return NX_ERR_TIMEOUT;
        }

        osal_yield();
    }
}

/**
 * \brief           Adapter receive_all implementation
 */
static nx_status_t rx_async_to_sync_receive_all(nx_rx_sync_t* self,
                                                uint8_t* data, size_t* len,
                                                uint32_t timeout_ms) {
    nx_rx_async_to_sync_adapter_t* adapter =
        NX_CONTAINER_OF(self, nx_rx_async_to_sync_adapter_t, sync);

    uint32_t start = nx_get_tick_ms();
    size_t requested = *len;
    size_t received = 0;

    /* Poll until all data received or timeout */
    while (received < requested) {
        size_t chunk_len = requested - received;
        nx_status_t status = adapter->async->receive(
            adapter->async, data + received, &chunk_len);

        if (status == NX_OK && chunk_len > 0) {
            received += chunk_len;
        }

        if (nx_get_tick_ms() - start > timeout_ms) {
            *len = received;
            return (received > 0) ? NX_ERR_TIMEOUT : NX_ERR_TIMEOUT;
        }

        if (status == NX_ERR_NO_DATA) {
            osal_yield();
        }
    }

    *len = received;
    return NX_OK;
}

/* Static adapter pool for async-to-sync RX */
static nx_rx_async_to_sync_adapter_t g_rx_a2s_adapters[4];
static uint8_t g_rx_a2s_used[4] = {0};

nx_rx_sync_t* nx_rx_async_to_sync(nx_rx_async_t* rx_async) {
    if (rx_async == NULL) {
        return NULL;
    }

    /* Find free adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_rx_a2s_adapters); i++) {
        if (!g_rx_a2s_used[i]) {
            g_rx_a2s_used[i] = 1;
            nx_rx_async_to_sync_adapter_t* adapter = &g_rx_a2s_adapters[i];

            adapter->async = rx_async;
            NX_INIT_RX_SYNC(&adapter->sync, rx_async_to_sync_receive,
                            rx_async_to_sync_receive_all);

            return &adapter->sync;
        }
    }

    return NULL; /* No free slots */
}

void nx_rx_async_to_sync_release(nx_rx_sync_t* adapter) {
    if (adapter == NULL) {
        return;
    }

    /* Find and release adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_rx_a2s_adapters); i++) {
        if (&g_rx_a2s_adapters[i].sync == adapter) {
            g_rx_a2s_used[i] = 0;
            memset(&g_rx_a2s_adapters[i], 0, sizeof(g_rx_a2s_adapters[i]));
            return;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Sync to Async TX Adapter                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync to async TX adapter structure
 */
typedef struct {
    nx_tx_async_t async; /**< Async interface (must be first) */
    nx_tx_sync_t* sync;  /**< Wrapped sync interface */
    uint32_t timeout_ms; /**< Default timeout */
    bool is_busy;        /**< Busy state */
} nx_tx_sync_to_async_adapter_t;

/**
 * \brief           Adapter send implementation
 */
static nx_status_t tx_sync_to_async_send(nx_tx_async_t* self,
                                         const uint8_t* data, size_t len) {
    nx_tx_sync_to_async_adapter_t* adapter =
        NX_CONTAINER_OF(self, nx_tx_sync_to_async_adapter_t, async);

    if (adapter->is_busy) {
        return NX_ERR_BUSY;
    }

    /* Use sync send with configured timeout */
    nx_status_t status =
        adapter->sync->send(adapter->sync, data, len, adapter->timeout_ms);

    /* Map timeout to busy (async semantics) */
    if (status == NX_ERR_TIMEOUT) {
        return NX_ERR_BUSY;
    }

    return status;
}

/**
 * \brief           Adapter get_state implementation
 */
static nx_status_t tx_sync_to_async_get_state(nx_tx_async_t* self) {
    nx_tx_sync_to_async_adapter_t* adapter =
        NX_CONTAINER_OF(self, nx_tx_sync_to_async_adapter_t, async);

    return adapter->is_busy ? NX_ERR_BUSY : NX_OK;
}

/* Static adapter pool for sync-to-async TX */
static nx_tx_sync_to_async_adapter_t g_tx_s2a_adapters[4];
static uint8_t g_tx_s2a_used[4] = {0};

nx_tx_async_t* nx_tx_sync_to_async(nx_tx_sync_t* tx_sync, uint32_t timeout_ms) {
    if (tx_sync == NULL) {
        return NULL;
    }

    /* Find free adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_tx_s2a_adapters); i++) {
        if (!g_tx_s2a_used[i]) {
            g_tx_s2a_used[i] = 1;
            nx_tx_sync_to_async_adapter_t* adapter = &g_tx_s2a_adapters[i];

            adapter->sync = tx_sync;
            adapter->timeout_ms = timeout_ms;
            adapter->is_busy = false;
            NX_INIT_TX_ASYNC(&adapter->async, tx_sync_to_async_send,
                             tx_sync_to_async_get_state);

            return &adapter->async;
        }
    }

    return NULL; /* No free slots */
}

void nx_tx_sync_to_async_release(nx_tx_async_t* adapter) {
    if (adapter == NULL) {
        return;
    }

    /* Find and release adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_tx_s2a_adapters); i++) {
        if (&g_tx_s2a_adapters[i].async == adapter) {
            g_tx_s2a_used[i] = 0;
            memset(&g_tx_s2a_adapters[i], 0, sizeof(g_tx_s2a_adapters[i]));
            return;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Sync to Async RX Adapter                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Sync to async RX adapter structure
 */
typedef struct {
    nx_rx_async_t async; /**< Async interface (must be first) */
    nx_rx_sync_t* sync;  /**< Wrapped sync interface */
    uint32_t timeout_ms; /**< Default timeout */
} nx_rx_sync_to_async_adapter_t;

/**
 * \brief           Adapter receive implementation
 */
static nx_status_t rx_sync_to_async_receive(nx_rx_async_t* self, uint8_t* data,
                                            size_t* len) {
    nx_rx_sync_to_async_adapter_t* adapter =
        NX_CONTAINER_OF(self, nx_rx_sync_to_async_adapter_t, async);

    /* Use sync receive with minimal timeout for non-blocking behavior */
    nx_status_t status = adapter->sync->receive(adapter->sync, data, len, 0);

    /* Map timeout to no_data (async semantics) */
    if (status == NX_ERR_TIMEOUT) {
        *len = 0;
        return NX_ERR_NO_DATA;
    }

    return status;
}

/* Static adapter pool for sync-to-async RX */
static nx_rx_sync_to_async_adapter_t g_rx_s2a_adapters[4];
static uint8_t g_rx_s2a_used[4] = {0};

nx_rx_async_t* nx_rx_sync_to_async(nx_rx_sync_t* rx_sync, uint32_t timeout_ms) {
    if (rx_sync == NULL) {
        return NULL;
    }

    /* Find free adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_rx_s2a_adapters); i++) {
        if (!g_rx_s2a_used[i]) {
            g_rx_s2a_used[i] = 1;
            nx_rx_sync_to_async_adapter_t* adapter = &g_rx_s2a_adapters[i];

            adapter->sync = rx_sync;
            adapter->timeout_ms = timeout_ms;
            NX_INIT_RX_ASYNC(&adapter->async, rx_sync_to_async_receive);

            return &adapter->async;
        }
    }

    return NULL; /* No free slots */
}

void nx_rx_sync_to_async_release(nx_rx_async_t* adapter) {
    if (adapter == NULL) {
        return;
    }

    /* Find and release adapter slot */
    for (size_t i = 0; i < NX_ARRAY_SIZE(g_rx_s2a_adapters); i++) {
        if (&g_rx_s2a_adapters[i].async == adapter) {
            g_rx_s2a_used[i] = 0;
            memset(&g_rx_s2a_adapters[i], 0, sizeof(g_rx_s2a_adapters[i]));
            return;
        }
    }
}

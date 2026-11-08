/**
 * \file            nxconfig.h
 * \brief           Readonly Host C11 configuration model and static emission
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_HOST_NXCONFIG_H
#define NEXUS_HOST_NXCONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Host failures never authorize a partially generated bundle. */
typedef enum {
    NXCONFIG_OK,
    NXCONFIG_INVALID,
    NXCONFIG_UNSUPPORTED,
    NXCONFIG_CONFLICT,
    NXCONFIG_IO,
    NXCONFIG_MEMORY
} nxconfig_result_t;

/** \brief Call-scoped error metadata; no supplied pointer is retained. */
typedef struct {
    nxconfig_result_t code;
    char source[512];
    char context[192];
    char message[512];
} nxconfig_diagnostic_t;

/** \brief Immutable assembly owner, released only through destroy. */
typedef struct nxconfig_assembly nxconfig_assembly_t;

/** \brief Classes implemented by the initial reviewed Host C path. */
typedef enum { NXCONFIG_GPIO, NXCONFIG_UART, NXCONFIG_SPI } nxconfig_kind_t;

/** \brief Exact reviewed silicon pad; not an inferred PCB connection. */
typedef struct {
    char name[5];
    uint8_t port;
    uint8_t number;
    uint8_t alternate;
} nxconfig_pin_t;

/** \brief GPIO electrical selections remain independent from pin identity. */
typedef struct {
    uint32_t mask;
    uint32_t initial;
    uint8_t pull;
    uint8_t output_type;
    bool output;
} nxconfig_gpio_t;

/** \brief UART ring units and IRQ policy are explicit static requirements. */
typedef struct {
    uint32_t baud;
    uint32_t clock_hz;
    uint32_t rx_capacity;
    char irq[48];
    uint8_t priority;
    bool events;
} nxconfig_uart_t;

/** \brief SPI controller budget; endpoint CS and mode remain separate. */
typedef struct {
    uint32_t clock_hz;
    uint32_t max_hz;
} nxconfig_spi_t;

/** \brief Named immutable controller record, never a generic MCU state union.
 */
typedef struct {
    nxconfig_kind_t kind;
    char id[64];
    char binding[64];
    char controller[16];
    char mode[32];
    char clock_domain[8];
    const nxconfig_pin_t* pins;
    size_t pin_count;
    union {
        nxconfig_gpio_t gpio;
        nxconfig_uart_t uart;
        nxconfig_spi_t spi;
    } options;
} nxconfig_controller_t;

/** \brief Addressed SPI endpoint, distinct from its owning controller. */
typedef struct {
    char id[64];
    size_t controller_index;
    nxconfig_pin_t chip_select;
    uint32_t max_hz;
    uint8_t mode;
} nxconfig_spi_endpoint_t;

/** \brief Selected identity and memory bounds; no qualification is inferred. */
typedef struct {
    char board[128];
    char part[32];
    char package[32];
    char device_define[32];
    char startup[96];
    char clock[64];
    char optimization[16];
    uint32_t flash_origin;
    uint32_t flash_bytes;
    uint32_t sram_origin;
    uint32_t sram_bytes;
    uint32_t core_hz;
    uint32_t apb1_hz;
    uint32_t apb2_hz;
    uint32_t main_stack_bytes;
    uint32_t static_ram_limit_bytes;
    uint32_t flash_load_limit_bytes;
    uint16_t external_irq_count;
    uint8_t irq_priority_bits;
} nxconfig_platform_t;

/** \brief Parse one schema-2 TOML using readonly F4 facts, without SDK access.
 * \param[in] source_root: Nexus source or Source SDK root.
 * \param[in] assembly_path: Authored file; relative paths use current
 * directory. \param[out] assembly: Cleared before work; receives an owner only
 * on success. \param[out] diagnostic: Optional call-scoped error, never
 * retained. \return Success or precise rejection; no firmware or output is
 * touched. \note Host-only, synchronous and allocation-owning; no target HAL is
 * called.
 */
nxconfig_result_t nxconfig_resolve(const char* source_root,
                                   const char* assembly_path,
                                   nxconfig_assembly_t** assembly,
                                   nxconfig_diagnostic_t* diagnostic);

/** \brief Release all host parser and IR storage; NULL is accepted. */
void nxconfig_destroy(nxconfig_assembly_t* assembly);
/** \brief Borrow readonly selected facts until assembly destruction. */
const nxconfig_platform_t*
nxconfig_platform(const nxconfig_assembly_t* assembly);
/** \brief Return the exact number of selected controllers. */
size_t nxconfig_controller_count(const nxconfig_assembly_t* assembly);
/** \brief Borrow one typed record, or NULL for an invalid index. */
const nxconfig_controller_t*
nxconfig_controller(const nxconfig_assembly_t* assembly, size_t index);
/** \brief Return the exact number of selected SPI device endpoints. */
size_t nxconfig_spi_endpoint_count(const nxconfig_assembly_t* assembly);
/** \brief Borrow one endpoint, or NULL for an invalid index. */
const nxconfig_spi_endpoint_t*
nxconfig_spi_endpoint(const nxconfig_assembly_t* assembly, size_t index);

/** \brief Publish only static C instances and fixed connection metadata.
 * \param[in] assembly: Successfully resolved immutable owner.
 * \param[in] output_directory: Dedicated output in an existing parent owned
 * by the current user, without group/other write access. Files use mode 0600;
 * unowned files are never erased. Writers must honor the lock; parent and
 * ancestor paths must remain stable throughout the call.
 * \param[out] diagnostic: Optional call-scoped error, never retained.
 * \return Success after complete publication; changed inputs are rejected.
 * \note Host-only. Emits no register algorithm, startup or scheduler behavior.
 */
nxconfig_result_t nxconfig_emit(const nxconfig_assembly_t* assembly,
                                const char* output_directory,
                                nxconfig_diagnostic_t* diagnostic);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_HOST_NXCONFIG_H */

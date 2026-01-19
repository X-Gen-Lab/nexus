/**
 * \file            nx_spi_i2c_example.c
 * \brief           Nexus HAL SPI and I2C Usage Examples
 * \author          Nexus Team
 *
 * This example demonstrates how to use the Nexus HAL SPI and I2C interfaces:
 * - SPI: Full-duplex transfer, transmit-only, receive-only
 * - SPI: Bus locking for multi-device access
 * - SPI: Runtime configuration changes
 * - I2C: Master transmit/receive
 * - I2C: Memory read/write operations
 * - I2C: Device probing and bus scanning
 *
 * \note            This example is for demonstration purposes and may need
 *                  adaptation for your specific platform and use case.
 */

#include "hal/nx_hal.h"
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* SPI Examples                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 1: Basic SPI full-duplex transfer
 * \details         Demonstrates simultaneous transmit and receive
 */
void example_spi_full_duplex(void) {
    printf("=== SPI Example 1: Full-Duplex Transfer ===\n");

    /* Get SPI device */
    nx_spi_t* spi = nx_factory_spi(0);
    if (!spi) {
        printf("Error: Failed to get SPI device\n");
        return;
    }

    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize SPI\n");
        nx_factory_spi_release(spi);
        return;
    }

    /* Prepare test data */
    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t rx_data[sizeof(tx_data)] = {0};

    /* Select chip */
    if (spi->cs_select(spi) != NX_OK) {
        printf("Error: Failed to select chip\n");
        goto cleanup;
    }

    /* Perform full-duplex transfer */
    nx_status_t status =
        spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000);
    if (status == NX_OK) {
        printf("Transfer successful\n");
        printf("TX: ");
        for (size_t i = 0; i < sizeof(tx_data); i++) {
            printf("0x%02X ", tx_data[i]);
        }
        printf("\nRX: ");
        for (size_t i = 0; i < sizeof(rx_data); i++) {
            printf("0x%02X ", rx_data[i]);
        }
        printf("\n");
    } else {
        printf("Transfer failed: %s\n", nx_status_to_string(status));
    }

    /* Deselect chip */
    spi->cs_deselect(spi);

cleanup:
    lifecycle->deinit(lifecycle);
    nx_factory_spi_release(spi);
    printf("\n");
}

/**
 * \brief           Example 2: SPI transmit-only and receive-only
 * \details         Demonstrates separate TX and RX operations
 */
void example_spi_tx_rx_separate(void) {
    printf("=== SPI Example 2: Separate TX/RX ===\n");

    nx_spi_t* spi = nx_factory_spi(0);
    if (!spi) {
        printf("Error: Failed to get SPI device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize SPI\n");
        nx_factory_spi_release(spi);
        return;
    }

    /* Transmit-only operation */
    printf("Transmit-only operation:\n");
    uint8_t tx_data[] = {0xAA, 0xBB, 0xCC, 0xDD};

    spi->cs_select(spi);
    nx_status_t status = spi->transmit(spi, tx_data, sizeof(tx_data), 1000);
    spi->cs_deselect(spi);

    if (status == NX_OK) {
        printf("  Transmitted %zu bytes\n", sizeof(tx_data));
    } else {
        printf("  Transmit failed: %s\n", nx_status_to_string(status));
    }

    /* Receive-only operation */
    printf("Receive-only operation:\n");
    uint8_t rx_data[4] = {0};

    spi->cs_select(spi);
    status = spi->receive(spi, rx_data, sizeof(rx_data), 1000);
    spi->cs_deselect(spi);

    if (status == NX_OK) {
        printf("  Received: ");
        for (size_t i = 0; i < sizeof(rx_data); i++) {
            printf("0x%02X ", rx_data[i]);
        }
        printf("\n");
    } else {
        printf("  Receive failed: %s\n", nx_status_to_string(status));
    }

    lifecycle->deinit(lifecycle);
    nx_factory_spi_release(spi);
    printf("\n");
}

/**
 * \brief           Example 3: SPI bus locking for multi-device access
 * \details         Demonstrates bus locking mechanism
 */
void example_spi_bus_lock(void) {
    printf("=== SPI Example 3: Bus Locking ===\n");

    nx_spi_t* spi = nx_factory_spi(0);
    if (!spi) {
        printf("Error: Failed to get SPI device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize SPI\n");
        nx_factory_spi_release(spi);
        return;
    }

    /* Lock the bus */
    printf("Locking SPI bus...\n");
    if (spi->lock(spi, 1000) != NX_OK) {
        printf("Error: Failed to lock bus\n");
        goto cleanup;
    }

    printf("Bus locked - performing multiple transactions\n");

    /* Transaction 1 */
    uint8_t tx1[] = {0x01, 0x02};
    uint8_t rx1[2] = {0};
    spi->cs_select(spi);
    spi->transfer(spi, tx1, rx1, sizeof(tx1), 1000);
    spi->cs_deselect(spi);
    printf("  Transaction 1 complete\n");

    /* Transaction 2 */
    uint8_t tx2[] = {0x03, 0x04};
    uint8_t rx2[2] = {0};
    spi->cs_select(spi);
    spi->transfer(spi, tx2, rx2, sizeof(tx2), 1000);
    spi->cs_deselect(spi);
    printf("  Transaction 2 complete\n");

    /* Unlock the bus */
    spi->unlock(spi);
    printf("Bus unlocked\n");

cleanup:
    lifecycle->deinit(lifecycle);
    nx_factory_spi_release(spi);
    printf("\n");
}

/**
 * \brief           Example 4: SPI runtime configuration (DEPRECATED)
 * \details         Demonstrates the old runtime configuration approach.
 *                  This is now deprecated - use Kconfig for compile-time
 * config.
 */
void example_spi_runtime_config(void) {
    printf("=== SPI Example 4: Runtime Configuration (DEPRECATED) ===\n");
    printf("Note: Runtime configuration is deprecated. Use Kconfig instead.\n");

    /* Get SPI device (configuration from Kconfig) */
    nx_spi_t* spi = nx_factory_spi(0);
    if (!spi) {
        printf("Error: Failed to get SPI device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize SPI\n");
        nx_factory_spi_release(spi);
        return;
    }

    /* Get current configuration */
    nx_spi_config_t current_config;
    if (spi->get_config(spi, &current_config) == NX_OK) {
        printf("Current configuration:\n");
        printf("  Clock: %lu Hz\n", (unsigned long)current_config.clock_hz);
        printf("  Mode: %d\n", current_config.mode);
        printf("  Bits: %d\n", current_config.bits);
    }

    /* Change clock speed */
    printf("\nChanging clock to 2 MHz...\n");
    if (spi->set_clock(spi, 2000000) == NX_OK) {
        spi->get_config(spi, &current_config);
        printf("New clock: %lu Hz\n", (unsigned long)current_config.clock_hz);
    }

    /* Change SPI mode */
    printf("Changing to SPI Mode 3...\n");
    if (spi->set_mode(spi, NX_SPI_MODE_3) == NX_OK) {
        spi->get_config(spi, &current_config);
        printf("New mode: %d\n", current_config.mode);
    }

    /* Get statistics */
    nx_spi_stats_t stats;
    if (spi->get_stats(spi, &stats) == NX_OK) {
        printf("\nSPI Statistics:\n");
        printf("  Busy: %s\n", stats.busy ? "Yes" : "No");
        printf("  TX count: %lu bytes\n", (unsigned long)stats.tx_count);
        printf("  RX count: %lu bytes\n", (unsigned long)stats.rx_count);
        printf("  Errors: %lu\n", (unsigned long)stats.error_count);
    }

    lifecycle->deinit(lifecycle);
    nx_factory_spi_release(spi);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* I2C Examples                                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Example 5: Basic I2C master transmit/receive
 * \details         Demonstrates basic I2C communication
 */
void example_i2c_basic(void) {
    printf("=== I2C Example 5: Basic Master TX/RX ===\n");

    /* Get I2C device */
    nx_i2c_t* i2c = nx_factory_i2c(0);
    if (!i2c) {
        printf("Error: Failed to get I2C device\n");
        return;
    }

    /* Initialize I2C */
    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize I2C\n");
        nx_factory_i2c_release(i2c);
        return;
    }

    /* I2C slave address (example: 0x50 for EEPROM) */
    uint16_t slave_addr = 0x50;

    /* Master transmit */
    printf("Master transmit to address 0x%02X:\n", slave_addr);
    uint8_t tx_data[] = {0x00, 0x10, 0xAA, 0xBB, 0xCC};
    nx_status_t status =
        i2c->master_transmit(i2c, slave_addr, tx_data, sizeof(tx_data), 1000);
    if (status == NX_OK) {
        printf("  Transmitted %zu bytes\n", sizeof(tx_data));
    } else {
        printf("  Transmit failed: %s\n", nx_status_to_string(status));
    }

    /* Master receive */
    printf("Master receive from address 0x%02X:\n", slave_addr);
    uint8_t rx_data[4] = {0};
    status =
        i2c->master_receive(i2c, slave_addr, rx_data, sizeof(rx_data), 1000);
    if (status == NX_OK) {
        printf("  Received: ");
        for (size_t i = 0; i < sizeof(rx_data); i++) {
            printf("0x%02X ", rx_data[i]);
        }
        printf("\n");
    } else {
        printf("  Receive failed: %s\n", nx_status_to_string(status));
    }

    lifecycle->deinit(lifecycle);
    nx_factory_i2c_release(i2c);
    printf("\n");
}

/**
 * \brief           Example 6: I2C memory read/write operations
 * \details         Demonstrates I2C memory operations (e.g., EEPROM access)
 */
void example_i2c_memory(void) {
    printf("=== I2C Example 6: Memory Read/Write ===\n");

    nx_i2c_t* i2c = nx_factory_i2c(0);
    if (!i2c) {
        printf("Error: Failed to get I2C device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize I2C\n");
        nx_factory_i2c_release(i2c);
        return;
    }

    uint16_t slave_addr = 0x50;
    uint16_t mem_addr = 0x0010; /* Memory address */
    uint8_t mem_addr_size = 2;  /* 2-byte address */

    /* Write to memory */
    printf("Writing to memory address 0x%04X:\n", mem_addr);
    uint8_t write_data[] = {0x11, 0x22, 0x33, 0x44};
    nx_status_t status =
        i2c->mem_write(i2c, slave_addr, mem_addr, mem_addr_size, write_data,
                       sizeof(write_data), 1000);
    if (status == NX_OK) {
        printf("  Wrote %zu bytes\n", sizeof(write_data));
    } else {
        printf("  Write failed: %s\n", nx_status_to_string(status));
    }

    /* Read from memory */
    printf("Reading from memory address 0x%04X:\n", mem_addr);
    uint8_t read_data[4] = {0};
    status = i2c->mem_read(i2c, slave_addr, mem_addr, mem_addr_size, read_data,
                           sizeof(read_data), 1000);
    if (status == NX_OK) {
        printf("  Read: ");
        for (size_t i = 0; i < sizeof(read_data); i++) {
            printf("0x%02X ", read_data[i]);
        }
        printf("\n");

        /* Verify data */
        if (memcmp(write_data, read_data, sizeof(write_data)) == 0) {
            printf("  Data verification: PASS\n");
        } else {
            printf("  Data verification: FAIL\n");
        }
    } else {
        printf("  Read failed: %s\n", nx_status_to_string(status));
    }

    lifecycle->deinit(lifecycle);
    nx_factory_i2c_release(i2c);
    printf("\n");
}

/**
 * \brief           Example 7: I2C device probing and bus scanning
 * \details         Demonstrates detecting devices on the I2C bus
 */
void example_i2c_scan(void) {
    printf("=== I2C Example 7: Device Probing and Bus Scan ===\n");

    nx_i2c_t* i2c = nx_factory_i2c(0);
    if (!i2c) {
        printf("Error: Failed to get I2C device\n");
        return;
    }

    nx_lifecycle_t* lifecycle = i2c->get_lifecycle(i2c);
    if (lifecycle->init(lifecycle) != NX_OK) {
        printf("Error: Failed to initialize I2C\n");
        nx_factory_i2c_release(i2c);
        return;
    }

    /* Probe specific device */
    uint16_t test_addr = 0x50;
    printf("Probing device at address 0x%02X...\n", test_addr);
    nx_status_t status = i2c->probe(i2c, test_addr, 100);
    if (status == NX_OK) {
        printf("  Device found at 0x%02X\n", test_addr);
    } else {
        printf("  No device at 0x%02X\n", test_addr);
    }

    /* Scan entire bus */
    printf("\nScanning I2C bus (0x00-0x7F)...\n");
    uint8_t found_addresses[128];
    size_t found_count = 0;

    status =
        i2c->scan(i2c, found_addresses, sizeof(found_addresses), &found_count);
    if (status == NX_OK) {
        printf("Found %zu device(s):\n", found_count);
        for (size_t i = 0; i < found_count; i++) {
            printf("  0x%02X\n", found_addresses[i]);
        }
    } else {
        printf("Bus scan failed: %s\n", nx_status_to_string(status));
    }

    lifecycle->deinit(lifecycle);
    nx_factory_i2c_release(i2c);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Main Function                                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Main function - runs all SPI and I2C examples
 */
int main(void) {
    printf("Nexus HAL SPI and I2C Examples\n");
    printf("===============================\n\n");

    /* Initialize HAL */
    nx_hal_init();

    /* Run SPI examples */
    printf("--- SPI Examples ---\n\n");
    example_spi_full_duplex();
    example_spi_tx_rx_separate();
    example_spi_bus_lock();
    example_spi_runtime_config();

    /* Run I2C examples */
    printf("--- I2C Examples ---\n\n");
    example_i2c_basic();
    example_i2c_memory();
    example_i2c_scan();

    /* Cleanup HAL */
    nx_hal_deinit();

    printf("All examples completed\n");
    return 0;
}

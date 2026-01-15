/**
 * \file            test_nx_spi.cpp
 * \brief           SPI Checkpoint Verification Tests (Task 10)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Checkpoint tests for SPI implementation:
 * - Test nx_spi_t transfer operations
 * - Test bus lock mechanism
 * - Test nx_spi_config_t runtime configuration
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/interface/nx_spi.h"

/* Factory function from native platform */
extern nx_spi_t* nx_spi_native_get(uint8_t index);
}

/**
 * \brief           SPI Checkpoint Test Fixture
 */
class NxSpiCheckpointTest : public ::testing::Test {
  protected:
    nx_spi_t* spi;

    void SetUp() override {
        spi = nx_spi_native_get(0);
        ASSERT_NE(nullptr, spi);
    }

    void TearDown() override {
        if (spi) {
            nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
            if (lifecycle) {
                lifecycle->deinit(lifecycle);
            }
        }
    }

    nx_spi_config_t makeDefaultConfig() {
        nx_spi_config_t cfg = {0};
        cfg.clock_hz = 1000000; /* 1 MHz */
        cfg.mode = NX_SPI_MODE_0;
        cfg.bits = 8;
        cfg.msb_first = true;
        cfg.cs_delay_us = 0;
        return cfg;
    }
};

/* ========== Transfer Operations Tests ========== */

/**
 * \brief           Test SPI transfer (full duplex)
 * \details         Checkpoint requirement: Test nx_spi_t transfer
 */
TEST_F(NxSpiCheckpointTest, TransferFullDuplex) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Prepare test data */
    const uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t rx_data[sizeof(tx_data)] = {0};

    /* Perform transfer */
    nx_status_t status =
        spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000);
    EXPECT_EQ(NX_OK, status);

    /* In native simulation, RX echoes TX */
    EXPECT_EQ(0, memcmp(tx_data, rx_data, sizeof(tx_data)));

    /* Verify statistics */
    nx_spi_stats_t stats;
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
    EXPECT_EQ(sizeof(tx_data), stats.rx_count);
}

/**
 * \brief           Test SPI transmit (TX only)
 * \details         Checkpoint requirement: Test nx_spi_t transfer
 */
TEST_F(NxSpiCheckpointTest, TransmitOnly) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Prepare test data */
    const uint8_t tx_data[] = {0xAA, 0xBB, 0xCC, 0xDD};

    /* Perform transmit */
    nx_status_t status = spi->transmit(spi, tx_data, sizeof(tx_data), 1000);
    EXPECT_EQ(NX_OK, status);

    /* Verify statistics */
    nx_spi_stats_t stats;
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(sizeof(tx_data), stats.tx_count);
}

/**
 * \brief           Test SPI receive (RX only)
 * \details         Checkpoint requirement: Test nx_spi_t transfer
 */
TEST_F(NxSpiCheckpointTest, ReceiveOnly) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Prepare receive buffer */
    uint8_t rx_data[8] = {0};

    /* Perform receive */
    nx_status_t status = spi->receive(spi, rx_data, sizeof(rx_data), 1000);
    EXPECT_EQ(NX_OK, status);

    /* In native simulation, RX gets 0xFF when TX is not provided */
    for (size_t i = 0; i < sizeof(rx_data); i++) {
        EXPECT_EQ(0xFF, rx_data[i]);
    }

    /* Verify statistics */
    nx_spi_stats_t stats;
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(sizeof(rx_data), stats.rx_count);
}

/**
 * \brief           Test CS select/deselect
 * \details         Checkpoint requirement: Test nx_spi_t transfer operations
 */
TEST_F(NxSpiCheckpointTest, ChipSelectControl) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test CS select */
    EXPECT_EQ(NX_OK, spi->cs_select(spi));

    /* Perform transfer with CS active */
    const uint8_t tx_data[] = {0x12, 0x34};
    uint8_t rx_data[sizeof(tx_data)] = {0};
    EXPECT_EQ(NX_OK,
              spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000));

    /* Test CS deselect */
    EXPECT_EQ(NX_OK, spi->cs_deselect(spi));
}

/**
 * \brief           Test multiple transfers
 * \details         Checkpoint requirement: Test nx_spi_t transfer
 */
TEST_F(NxSpiCheckpointTest, MultipleTransfers) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Perform multiple transfers */
    for (int i = 0; i < 5; i++) {
        uint8_t tx_data[] = {(uint8_t)i, (uint8_t)(i + 1), (uint8_t)(i + 2)};
        uint8_t rx_data[sizeof(tx_data)] = {0};

        EXPECT_EQ(NX_OK,
                  spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000));
        EXPECT_EQ(0, memcmp(tx_data, rx_data, sizeof(tx_data)));
    }

    /* Verify cumulative statistics */
    nx_spi_stats_t stats;
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(15u, stats.tx_count); /* 5 transfers * 3 bytes */
    EXPECT_EQ(15u, stats.rx_count);
}

/* ========== Bus Lock Tests ========== */

/**
 * \brief           Test bus lock acquisition
 * \details         Checkpoint requirement: Test bus lock
 */
TEST_F(NxSpiCheckpointTest, BusLockAcquisition) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Lock the bus */
    EXPECT_EQ(NX_OK, spi->lock(spi, 1000));

    /* Try to lock again - should fail */
    EXPECT_EQ(NX_ERR_LOCKED, spi->lock(spi, 1000));

    /* Unlock the bus */
    EXPECT_EQ(NX_OK, spi->unlock(spi));
}

/**
 * \brief           Test bus lock prevents transfer
 * \details         Checkpoint requirement: Test bus lock
 */
TEST_F(NxSpiCheckpointTest, BusLockPreventsTransfer) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Lock the bus */
    ASSERT_EQ(NX_OK, spi->lock(spi, 1000));

    /* Try to transfer - should fail */
    const uint8_t tx_data[] = {0x01, 0x02};
    uint8_t rx_data[sizeof(tx_data)] = {0};
    EXPECT_EQ(NX_ERR_LOCKED,
              spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000));

    /* Unlock and try again - should succeed */
    ASSERT_EQ(NX_OK, spi->unlock(spi));
    EXPECT_EQ(NX_OK,
              spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000));
}

/**
 * \brief           Test bus lock/unlock cycle
 * \details         Checkpoint requirement: Test bus lock
 */
TEST_F(NxSpiCheckpointTest, BusLockUnlockCycle) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Perform multiple lock/unlock cycles */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(NX_OK, spi->lock(spi, 1000)) << "Lock failed on cycle " << i;
        EXPECT_EQ(NX_OK, spi->unlock(spi)) << "Unlock failed on cycle " << i;
    }
}

/**
 * \brief           Test unlock without lock fails
 * \details         Checkpoint requirement: Test bus lock error handling
 */
TEST_F(NxSpiCheckpointTest, UnlockWithoutLockFails) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Try to unlock without locking - should fail */
    EXPECT_EQ(NX_ERR_INVALID_STATE, spi->unlock(spi));
}

/**
 * \brief           Test bus lock with transfer sequence
 * \details         Checkpoint requirement: Test bus lock with operations
 */
TEST_F(NxSpiCheckpointTest, BusLockWithTransferSequence) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Perform transfer without lock */
    const uint8_t tx_data1[] = {0x11, 0x22};
    uint8_t rx_data1[sizeof(tx_data1)] = {0};
    EXPECT_EQ(NX_OK,
              spi->transfer(spi, tx_data1, rx_data1, sizeof(tx_data1), 1000));

    /* Lock, transfer, unlock */
    ASSERT_EQ(NX_OK, spi->lock(spi, 1000));

    /* Transfer should fail while locked */
    const uint8_t tx_data2[] = {0x33, 0x44};
    uint8_t rx_data2[sizeof(tx_data2)] = {0};
    EXPECT_EQ(NX_ERR_LOCKED,
              spi->transfer(spi, tx_data2, rx_data2, sizeof(tx_data2), 1000));

    ASSERT_EQ(NX_OK, spi->unlock(spi));

    /* Transfer should succeed after unlock */
    EXPECT_EQ(NX_OK,
              spi->transfer(spi, tx_data2, rx_data2, sizeof(tx_data2), 1000));
}

/* ========== Runtime Configuration Tests ========== */

/**
 * \brief           Test runtime clock configuration
 * \details         Checkpoint requirement: Test nx_spi_config_t runtime
 * configuration
 */
TEST_F(NxSpiCheckpointTest, RuntimeClockConfiguration) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Verify initial clock */
    nx_spi_config_t cfg;
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg));
    EXPECT_EQ(1000000u, cfg.clock_hz);

    /* Change clock to 2 MHz */
    ASSERT_EQ(NX_OK, spi->set_clock(spi, 2000000));

    /* Verify clock changed */
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg));
    EXPECT_EQ(2000000u, cfg.clock_hz);

    /* Change clock to 500 kHz */
    ASSERT_EQ(NX_OK, spi->set_clock(spi, 500000));

    /* Verify clock changed again */
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg));
    EXPECT_EQ(500000u, cfg.clock_hz);
}

/**
 * \brief           Test runtime mode configuration
 * \details         Checkpoint requirement: Test nx_spi_config_t runtime
 * configuration
 */
TEST_F(NxSpiCheckpointTest, RuntimeModeConfiguration) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test all SPI modes */
    nx_spi_mode_t modes[] = {NX_SPI_MODE_0, NX_SPI_MODE_1, NX_SPI_MODE_2,
                             NX_SPI_MODE_3};

    for (nx_spi_mode_t mode : modes) {
        ASSERT_EQ(NX_OK, spi->set_mode(spi, mode))
            << "Failed to set mode " << mode;

        nx_spi_config_t cfg;
        ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg));
        EXPECT_EQ(mode, cfg.mode) << "Mode mismatch for " << mode;
    }
}

/**
 * \brief           Test complete configuration get/set
 * \details         Checkpoint requirement: Test nx_spi_config_t runtime
 * configuration
 */
TEST_F(NxSpiCheckpointTest, CompleteConfigurationGetSet) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Create test configuration */
    nx_spi_config_t cfg_write = {0};
    cfg_write.clock_hz = 4000000; /* 4 MHz */
    cfg_write.mode = NX_SPI_MODE_3;
    cfg_write.bits = 16;
    cfg_write.msb_first = false;
    cfg_write.cs_delay_us = 10;

    /* Set configuration */
    ASSERT_EQ(NX_OK, spi->set_config(spi, &cfg_write));

    /* Get configuration back */
    nx_spi_config_t cfg_read = {0};
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg_read));

    /* Verify round-trip consistency */
    EXPECT_EQ(cfg_write.clock_hz, cfg_read.clock_hz);
    EXPECT_EQ(cfg_write.mode, cfg_read.mode);
    EXPECT_EQ(cfg_write.bits, cfg_read.bits);
    EXPECT_EQ(cfg_write.msb_first, cfg_read.msb_first);
    EXPECT_EQ(cfg_write.cs_delay_us, cfg_read.cs_delay_us);
}

/**
 * \brief           Test configuration round-trip with various settings
 * \details         Checkpoint requirement: Test nx_spi_config_t runtime
 * configuration
 */
TEST_F(NxSpiCheckpointTest, ConfigurationRoundTripVariations) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Test configuration 1: Low speed, mode 0 */
    nx_spi_config_t cfg1 = {0};
    cfg1.clock_hz = 100000; /* 100 kHz */
    cfg1.mode = NX_SPI_MODE_0;
    cfg1.bits = 8;
    cfg1.msb_first = true;
    cfg1.cs_delay_us = 0;

    ASSERT_EQ(NX_OK, spi->set_config(spi, &cfg1));
    nx_spi_config_t cfg1_read = {0};
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg1_read));
    EXPECT_EQ(0, memcmp(&cfg1, &cfg1_read, sizeof(nx_spi_config_t)));

    /* Test configuration 2: High speed, mode 3 */
    nx_spi_config_t cfg2 = {0};
    cfg2.clock_hz = 10000000; /* 10 MHz */
    cfg2.mode = NX_SPI_MODE_3;
    cfg2.bits = 16;
    cfg2.msb_first = false;
    cfg2.cs_delay_us = 100;

    ASSERT_EQ(NX_OK, spi->set_config(spi, &cfg2));
    nx_spi_config_t cfg2_read = {0};
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg2_read));
    EXPECT_EQ(0, memcmp(&cfg2, &cfg2_read, sizeof(nx_spi_config_t)));
}

/**
 * \brief           Test transfer after configuration change
 * \details         Checkpoint requirement: Test operations after runtime config
 */
TEST_F(NxSpiCheckpointTest, TransferAfterConfigurationChange) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Transfer with initial config */
    const uint8_t tx_data1[] = {0xAA, 0xBB};
    uint8_t rx_data1[sizeof(tx_data1)] = {0};
    EXPECT_EQ(NX_OK,
              spi->transfer(spi, tx_data1, rx_data1, sizeof(tx_data1), 1000));

    /* Change configuration */
    ASSERT_EQ(NX_OK, spi->set_clock(spi, 2000000));
    ASSERT_EQ(NX_OK, spi->set_mode(spi, NX_SPI_MODE_2));

    /* Transfer with new config */
    const uint8_t tx_data2[] = {0xCC, 0xDD};
    uint8_t rx_data2[sizeof(tx_data2)] = {0};
    EXPECT_EQ(NX_OK,
              spi->transfer(spi, tx_data2, rx_data2, sizeof(tx_data2), 1000));

    /* Verify both transfers succeeded */
    EXPECT_EQ(0, memcmp(tx_data1, rx_data1, sizeof(tx_data1)));
    EXPECT_EQ(0, memcmp(tx_data2, rx_data2, sizeof(tx_data2)));
}

/* ========== Lifecycle and Power Tests ========== */

/**
 * \brief           Test lifecycle management
 * \details         Verify lifecycle operations work correctly
 */
TEST_F(NxSpiCheckpointTest, LifecycleManagement) {
    /* Get lifecycle interface */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);

    /* Check initial state */
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Initialize */
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Try to initialize again - should fail */
    EXPECT_EQ(NX_ERR_ALREADY_INIT, lifecycle->init(lifecycle));

    /* Suspend */
    EXPECT_EQ(NX_OK, lifecycle->suspend(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_SUSPENDED, lifecycle->get_state(lifecycle));

    /* Resume */
    EXPECT_EQ(NX_OK, lifecycle->resume(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_RUNNING, lifecycle->get_state(lifecycle));

    /* Deinitialize */
    EXPECT_EQ(NX_OK, lifecycle->deinit(lifecycle));
    EXPECT_EQ(NX_DEV_STATE_UNINITIALIZED, lifecycle->get_state(lifecycle));

    /* Try to deinitialize again - should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, lifecycle->deinit(lifecycle));
}

/**
 * \brief           Test suspend/resume preserves configuration
 * \details         Verify config is preserved across suspend/resume
 */
TEST_F(NxSpiCheckpointTest, SuspendResumePreservesConfig) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Set custom configuration */
    nx_spi_config_t cfg_before = {0};
    cfg_before.clock_hz = 3000000;
    cfg_before.mode = NX_SPI_MODE_2;
    cfg_before.bits = 16;
    cfg_before.msb_first = false;
    cfg_before.cs_delay_us = 50;

    ASSERT_EQ(NX_OK, spi->set_config(spi, &cfg_before));

    /* Suspend */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Resume */
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Verify configuration preserved */
    nx_spi_config_t cfg_after = {0};
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg_after));
    EXPECT_EQ(0, memcmp(&cfg_before, &cfg_after, sizeof(nx_spi_config_t)));
}

/**
 * \brief           Test power management
 * \details         Verify power operations
 */
TEST_F(NxSpiCheckpointTest, PowerManagement) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    EXPECT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get power interface */
    nx_power_t* power = spi->get_power(spi);
    ASSERT_NE(nullptr, power);

    /* Check power is enabled after init */
    EXPECT_TRUE(power->is_enabled(power));

    /* Disable power */
    EXPECT_EQ(NX_OK, power->disable(power));
    EXPECT_FALSE(power->is_enabled(power));

    /* Enable power */
    EXPECT_EQ(NX_OK, power->enable(power));
    EXPECT_TRUE(power->is_enabled(power));
}

/**
 * \brief           Test operations on uninitialized SPI
 * \details         Verify error handling for uninitialized device
 */
TEST_F(NxSpiCheckpointTest, UninitializedOperations) {
    /* Don't initialize - test operations on uninitialized device */

    /* Transfer should fail */
    const uint8_t tx_data[] = {0x01, 0x02};
    uint8_t rx_data[sizeof(tx_data)] = {0};
    EXPECT_EQ(NX_ERR_NOT_INIT,
              spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000));

    /* Configuration operations should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->set_clock(spi, 1000000));
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->set_mode(spi, NX_SPI_MODE_0));

    nx_spi_config_t config;
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->get_config(spi, &config));
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->set_config(spi, &config));

    /* Lock operations should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->lock(spi, 1000));
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->unlock(spi));

    /* CS operations should fail */
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->cs_select(spi));
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->cs_deselect(spi));

    /* Stats should fail */
    nx_spi_stats_t stats;
    EXPECT_EQ(NX_ERR_NOT_INIT, spi->get_stats(spi, &stats));
}

/**
 * \brief           Test statistics tracking
 * \details         Verify statistics are correctly tracked
 */
TEST_F(NxSpiCheckpointTest, StatisticsTracking) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Check initial statistics */
    nx_spi_stats_t stats;
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(0u, stats.tx_count);
    EXPECT_EQ(0u, stats.rx_count);
    EXPECT_EQ(0u, stats.error_count);
    EXPECT_FALSE(stats.busy);

    /* Perform transfers */
    const uint8_t tx_data1[] = {0x01, 0x02, 0x03};
    uint8_t rx_data1[sizeof(tx_data1)] = {0};
    ASSERT_EQ(NX_OK,
              spi->transfer(spi, tx_data1, rx_data1, sizeof(tx_data1), 1000));

    const uint8_t tx_data2[] = {0x04, 0x05};
    uint8_t rx_data2[sizeof(tx_data2)] = {0};
    ASSERT_EQ(NX_OK,
              spi->transfer(spi, tx_data2, rx_data2, sizeof(tx_data2), 1000));

    /* Check updated statistics */
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(5u, stats.tx_count); /* 3 + 2 */
    EXPECT_EQ(5u, stats.rx_count); /* 3 + 2 */
}

/**
 * \brief           Test diagnostic interface
 * \details         Verify diagnostic operations
 */
TEST_F(NxSpiCheckpointTest, DiagnosticInterface) {
    /* Initialize SPI */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Get diagnostic interface */
    nx_diagnostic_t* diag = spi->get_diagnostic(spi);
    ASSERT_NE(nullptr, diag);

    /* Get status */
    nx_spi_stats_t status;
    EXPECT_EQ(NX_OK, diag->get_status(diag, &status, sizeof(status)));

    /* Get statistics */
    nx_spi_stats_t stats;
    EXPECT_EQ(NX_OK, diag->get_statistics(diag, &stats, sizeof(stats)));

    /* Clear statistics */
    EXPECT_EQ(NX_OK, diag->clear_statistics(diag));

    /* Verify cleared */
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(0u, stats.tx_count);
    EXPECT_EQ(0u, stats.rx_count);
    EXPECT_EQ(0u, stats.error_count);
}

/**
 * \brief           Test full workflow integration
 * \details         Integration test of all features
 */
TEST_F(NxSpiCheckpointTest, FullWorkflowIntegration) {
    /* Initialize */
    nx_lifecycle_t* lifecycle = spi->get_lifecycle(spi);
    ASSERT_NE(nullptr, lifecycle);
    ASSERT_EQ(NX_OK, lifecycle->init(lifecycle));

    /* Configure */
    nx_spi_config_t cfg = makeDefaultConfig();
    cfg.clock_hz = 2000000;
    cfg.mode = NX_SPI_MODE_1;
    ASSERT_EQ(NX_OK, spi->set_config(spi, &cfg));

    /* Lock bus */
    ASSERT_EQ(NX_OK, spi->lock(spi, 1000));
    ASSERT_EQ(NX_OK, spi->unlock(spi));

    /* Transfer data */
    const uint8_t tx_data[] = {0x11, 0x22, 0x33};
    uint8_t rx_data[sizeof(tx_data)] = {0};
    ASSERT_EQ(NX_OK,
              spi->transfer(spi, tx_data, rx_data, sizeof(tx_data), 1000));

    /* Change configuration */
    ASSERT_EQ(NX_OK, spi->set_clock(spi, 4000000));

    /* Suspend */
    ASSERT_EQ(NX_OK, lifecycle->suspend(lifecycle));

    /* Resume */
    ASSERT_EQ(NX_OK, lifecycle->resume(lifecycle));

    /* Transfer after resume */
    const uint8_t tx_data2[] = {0x44, 0x55};
    uint8_t rx_data2[sizeof(tx_data2)] = {0};
    ASSERT_EQ(NX_OK,
              spi->transfer(spi, tx_data2, rx_data2, sizeof(tx_data2), 1000));

    /* Verify configuration preserved */
    nx_spi_config_t cfg_final;
    ASSERT_EQ(NX_OK, spi->get_config(spi, &cfg_final));
    EXPECT_EQ(4000000u, cfg_final.clock_hz);

    /* Verify statistics */
    nx_spi_stats_t stats;
    ASSERT_EQ(NX_OK, spi->get_stats(spi, &stats));
    EXPECT_EQ(5u, stats.tx_count); /* 3 + 2 */
    EXPECT_EQ(5u, stats.rx_count);
}

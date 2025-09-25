// Mock SPI framework for testing Matrix Creator modules
#include <kunit/test.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/module.h>
#include "mock-spi.h"

static struct spi_device *mock_spi_device;
static struct mock_spi_data mock_data;

// Mock SPI transfer function
int mock_spi_sync(struct spi_device *spi, struct spi_message *message)
{
    struct spi_transfer *transfer;
    int ret = 0;
    
    if (!spi || !message) {
        return -EINVAL;
    }
    
    mock_data.transfer_count++;
    mock_data.last_message = message;
    
    // Simulate transfer for each transfer in the message
    list_for_each_entry(transfer, &message->transfers, transfer_list) {
        mock_data.last_transfer = transfer;
        mock_data.total_bytes_transferred += transfer->len;
        
        // Copy mock response data if this is a read operation
        if (transfer->rx_buf && mock_data.mock_response_data) {
            memcpy(transfer->rx_buf, mock_data.mock_response_data, 
                   min(transfer->len, mock_data.mock_response_len));
        }
        
        // Store write data for verification
        if (transfer->tx_buf && transfer->len <= MOCK_MAX_TRANSFER_SIZE) {
            memcpy(mock_data.last_tx_data, transfer->tx_buf, transfer->len);
            mock_data.last_tx_len = transfer->len;
        }
    }
    
    // Simulate errors if configured
    if (mock_data.simulate_error) {
        ret = mock_data.error_code;
        mock_data.simulate_error = false;
    }
    
    return ret;
}

// Mock SPI setup function
int mock_spi_setup(struct spi_device *spi)
{
    if (!spi) {
        return -EINVAL;
    }
    
    mock_data.setup_called = true;
    return 0;
}

// Create a mock SPI device for testing
struct spi_device *create_mock_spi_device(struct kunit *test)
{
    struct spi_device *spi;
    struct spi_master *master;
    
    master = kunit_kzalloc(test, sizeof(*master), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, master);
    
    spi = kunit_kzalloc(test, sizeof(*spi), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    spi->master = master;
    spi->max_speed_hz = 1000000;
    spi->chip_select = 0;
    spi->mode = SPI_MODE_0;
    spi->bits_per_word = 8;
    
    // Set up device structure
    device_initialize(&spi->dev);
    dev_set_name(&spi->dev, "mock_spi");
    
    mock_spi_device = spi;
    reset_mock_spi_data();
    
    return spi;
}

// Reset mock data for clean test state
void reset_mock_spi_data(void)
{
    memset(&mock_data, 0, sizeof(mock_data));
}

// Configure mock to simulate errors
void set_mock_spi_error(int error_code)
{
    mock_data.simulate_error = true;
    mock_data.error_code = error_code;
}

// Set mock response data for read operations
void set_mock_spi_response(const void *data, size_t len)
{
    mock_data.mock_response_data = data;
    mock_data.mock_response_len = len;
}

// Get mock data for verification in tests
struct mock_spi_data *get_mock_spi_data(void)
{
    return &mock_data;
}

// Verify that expected SPI operations occurred
void verify_spi_transfer(struct kunit *test, size_t expected_transfers, 
                        size_t expected_bytes)
{
    KUNIT_EXPECT_EQ(test, mock_data.transfer_count, expected_transfers);
    KUNIT_EXPECT_EQ(test, mock_data.total_bytes_transferred, expected_bytes);
}

// Verify transmitted data
void verify_spi_tx_data(struct kunit *test, const void *expected_data, 
                       size_t expected_len)
{
    KUNIT_EXPECT_EQ(test, mock_data.last_tx_len, expected_len);
    KUNIT_EXPECT_MEMEQ(test, mock_data.last_tx_data, expected_data, expected_len);
}
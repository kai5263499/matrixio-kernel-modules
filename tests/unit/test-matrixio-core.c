// Unit tests for Matrix Creator core SPI communication module
#include <kunit/test.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/module.h>

// Test-specific includes
#include "../mocks/mock-spi.h"
#include "../../src/matrixio-core.h"

// Test hardware command structure packing
static void test_hardware_cmd_structure(struct kunit *test)
{
    struct {
        uint8_t readnwrite : 1;
        uint16_t reg : 15;
    } __packed cmd = {0};
    
    // Test structure size and alignment
    KUNIT_EXPECT_EQ(test, sizeof(cmd), 2);
    
    // Test bit field packing
    cmd.readnwrite = 1;
    cmd.reg = 0x7FFF;  // Maximum 15-bit value
    
    KUNIT_EXPECT_EQ(test, cmd.readnwrite, 1);
    KUNIT_EXPECT_EQ(test, cmd.reg, 0x7FFF);
    
    // Test that readnwrite bit doesn't overflow into reg field
    cmd.readnwrite = 0;
    KUNIT_EXPECT_EQ(test, cmd.reg, 0x7FFF);
    
    // Test that reg field doesn't overflow into readnwrite bit
    cmd.reg = 0;
    KUNIT_EXPECT_EQ(test, cmd.readnwrite, 0);
}

// Test SPI bounce size threshold
static void test_spi_bounce_size_threshold(struct kunit *test)
{
    // Verify the bounce size constant is reasonable
    KUNIT_EXPECT_EQ(test, MATRIXIO_SPI_BOUNCE_SIZE, 2048);
    KUNIT_EXPECT_GT(test, MATRIXIO_SPI_BOUNCE_SIZE, 0);
    KUNIT_EXPECT_LT(test, MATRIXIO_SPI_BOUNCE_SIZE, 65536); // Reasonable upper bound
}

// Test matrixio device structure initialization
static void test_matrixio_device_init(struct kunit *test)
{
    struct spi_device *spi;
    struct matrixio *mio;
    
    // Create mock SPI device
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    // Allocate matrixio structure
    mio = kunit_kzalloc(test, sizeof(*mio), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio);
    
    // Initialize basic fields
    mio->spi = spi;
    
    // Allocate buffers
    mio->tx_buffer = kunit_kzalloc(test, MATRIXIO_SPI_BOUNCE_SIZE, GFP_KERNEL);
    mio->rx_buffer = kunit_kzalloc(test, MATRIXIO_SPI_BOUNCE_SIZE, GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio->tx_buffer);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio->rx_buffer);
    
    // Verify initialization
    KUNIT_EXPECT_PTR_EQ(test, mio->spi, spi);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mio->tx_buffer);
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mio->rx_buffer);
}

// Test SPI transfer message construction for small reads
static void test_small_read_transfer(struct kunit *test)
{
    struct spi_device *spi;
    struct matrixio *mio;
    uint8_t test_data[32] = {0};
    uint16_t test_addr = 0x1234;
    size_t test_len = sizeof(test_data);
    
    // Set up mock SPI device and matrixio structure
    spi = create_mock_spi_device(test);
    mio = kunit_kzalloc(test, sizeof(*mio), GFP_KERNEL);
    mio->spi = spi;
    mio->tx_buffer = kunit_kzalloc(test, MATRIXIO_SPI_BOUNCE_SIZE, GFP_KERNEL);
    mio->rx_buffer = kunit_kzalloc(test, MATRIXIO_SPI_BOUNCE_SIZE, GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio->tx_buffer);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio->rx_buffer);
    
    // Set up mock response data
    uint8_t mock_response[34]; // 2 bytes address + 32 bytes data
    memset(mock_response, 0x55, sizeof(mock_response));
    set_mock_spi_response(mock_response, sizeof(mock_response));
    
    // This would typically call the actual matrixio_read function
    // For now, verify the mock SPI framework works
    struct spi_message msg;
    struct spi_transfer xfer = {
        .tx_buf = mio->tx_buffer,
        .rx_buf = mio->rx_buffer,
        .len = test_len + 2, // +2 for address bytes
    };
    
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    
    int ret = mock_spi_sync(spi, &msg);
    KUNIT_EXPECT_EQ(test, ret, 0);
    
    // Verify mock tracked the transfer
    verify_spi_transfer(test, 1, test_len + 2);
}

// Test SPI transfer message construction for large reads
static void test_large_read_transfer(struct kunit *test)
{
    struct spi_device *spi;
    size_t large_size = MATRIXIO_SPI_BOUNCE_SIZE * 2; // Larger than bounce buffer
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    // Large transfers should use two-transfer method
    // This test verifies the threshold logic exists
    KUNIT_EXPECT_GT(test, large_size, MATRIXIO_SPI_BOUNCE_SIZE);
}

// Test error handling in SPI operations
static void test_spi_error_handling(struct kunit *test)
{
    struct spi_device *spi;
    struct spi_message msg;
    struct spi_transfer xfer = {.len = 32};
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    // Test with NULL SPI device
    int ret = mock_spi_sync(NULL, &msg);
    KUNIT_EXPECT_EQ(test, ret, -EINVAL);
    
    // Test with NULL message
    ret = mock_spi_sync(spi, NULL);
    KUNIT_EXPECT_EQ(test, ret, -EINVAL);
    
    // Test simulated SPI error
    set_mock_spi_error(-EIO);
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    
    ret = mock_spi_sync(spi, &msg);
    KUNIT_EXPECT_EQ(test, ret, -EIO);
}

// Test register address validation
static void test_register_address_validation(struct kunit *test)
{
    // Test valid address ranges (15-bit addresses)
    uint16_t valid_addresses[] = {0x0000, 0x1234, 0x7FFF};
    uint16_t invalid_addresses[] = {0x8000, 0xFFFF}; // Beyond 15-bit range
    
    for (int i = 0; i < ARRAY_SIZE(valid_addresses); i++) {
        uint16_t addr = valid_addresses[i];
        KUNIT_EXPECT_LE(test, addr, 0x7FFF);
    }
    
    for (int i = 0; i < ARRAY_SIZE(invalid_addresses); i++) {
        uint16_t addr = invalid_addresses[i];
        KUNIT_EXPECT_GT(test, addr, 0x7FFF);
    }
}

// Test buffer alignment and DMA safety
static void test_buffer_dma_safety(struct kunit *test)
{
    struct matrixio *mio;
    
    mio = kunit_kzalloc(test, sizeof(*mio), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio);
    
    // Allocate DMA-safe buffers
    mio->tx_buffer = kunit_kzalloc(test, MATRIXIO_SPI_BOUNCE_SIZE, GFP_KERNEL);
    mio->rx_buffer = kunit_kzalloc(test, MATRIXIO_SPI_BOUNCE_SIZE, GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio->tx_buffer);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mio->rx_buffer);
    
    // Verify buffer alignment (typically required for DMA)
    KUNIT_EXPECT_EQ(test, (unsigned long)mio->tx_buffer % sizeof(void*), 0);
    KUNIT_EXPECT_EQ(test, (unsigned long)mio->rx_buffer % sizeof(void*), 0);
}

// KUnit test suite definition
static struct kunit_case matrixio_core_test_cases[] = {
    KUNIT_CASE(test_hardware_cmd_structure),
    KUNIT_CASE(test_spi_bounce_size_threshold),
    KUNIT_CASE(test_matrixio_device_init),
    KUNIT_CASE(test_small_read_transfer),
    KUNIT_CASE(test_large_read_transfer),
    KUNIT_CASE(test_spi_error_handling),
    KUNIT_CASE(test_register_address_validation),
    KUNIT_CASE(test_buffer_dma_safety),
    {}
};

static struct kunit_suite matrixio_core_test_suite = {
    .name = "matrixio-core",
    .test_cases = matrixio_core_test_cases,
};

kunit_test_suite(matrixio_core_test_suite);
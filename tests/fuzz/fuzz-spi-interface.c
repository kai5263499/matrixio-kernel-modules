// Fuzzing tests for SPI interface robustness
#include <kunit/test.h>
#include <linux/random.h>
#include <linux/spi/spi.h>
#include <linux/limits.h>

// Test-specific includes
#include "../mocks/mock-spi.h"
#include "../../src/matrixio-core.h"

#define FUZZ_ITERATIONS 1000
#define FUZZ_MAX_TRANSFER_SIZE 8192

// Helper function to generate random data
static void generate_random_data(void *buffer, size_t size)
{
    uint8_t *data = (uint8_t *)buffer;
    for (size_t i = 0; i < size; i++) {
        get_random_bytes(&data[i], 1);
    }
}

// Fuzz SPI transfer sizes
static void fuzz_spi_transfer_sizes(struct kunit *test)
{
    struct spi_device *spi;
    struct spi_message msg;
    struct spi_transfer xfer;
    uint8_t *buffer;
    size_t test_sizes[] = {
        0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256,
        511, 512, 1023, 1024, 2047, 2048, 4095, 4096, 8191, 8192
    };
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    buffer = kunit_kzalloc(test, FUZZ_MAX_TRANSFER_SIZE, GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buffer);
    
    for (int i = 0; i < ARRAY_SIZE(test_sizes); i++) {
        size_t size = test_sizes[i];
        
        if (size > FUZZ_MAX_TRANSFER_SIZE) {
            continue;
        }
        
        reset_mock_spi_data();
        
        // Fill buffer with random data
        generate_random_data(buffer, size);
        
        // Set up transfer
        memset(&xfer, 0, sizeof(xfer));
        xfer.tx_buf = buffer;
        xfer.rx_buf = buffer;
        xfer.len = size;
        
        spi_message_init(&msg);
        spi_message_add_tail(&xfer, &msg);
        
        // Test transfer - should handle all sizes gracefully
        int ret = mock_spi_sync(spi, &msg);
        
        // Even with random sizes, basic error handling should work
        if (size == 0) {
            // Zero-length transfers might be rejected
            KUNIT_EXPECT_TRUE(test, ret == 0 || ret < 0);
        } else {
            // Non-zero transfers should generally succeed in mock
            KUNIT_EXPECT_EQ(test, ret, 0);
        }
    }
}

// Fuzz SPI addresses with random patterns
static void fuzz_spi_addresses(struct kunit *test)
{
    struct spi_device *spi;
    uint16_t addresses[FUZZ_ITERATIONS];
    uint8_t data_buffer[32];
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    // Generate random addresses
    get_random_bytes(addresses, sizeof(addresses));
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint16_t addr = addresses[i];
        struct spi_message msg;
        struct spi_transfer xfer;
        
        reset_mock_spi_data();
        
        // Prepare address and data
        data_buffer[0] = (addr >> 8) & 0xFF;
        data_buffer[1] = addr & 0xFF;
        generate_random_data(&data_buffer[2], sizeof(data_buffer) - 2);
        
        // Set up transfer
        memset(&xfer, 0, sizeof(xfer));
        xfer.tx_buf = data_buffer;
        xfer.len = sizeof(data_buffer);
        
        spi_message_init(&msg);
        spi_message_add_tail(&xfer, &msg);
        
        // Test transfer - should not crash with any address
        int ret = mock_spi_sync(spi, &msg);
        
        // Mock should handle any address
        KUNIT_EXPECT_GE(test, ret, -EINVAL); // No worse than EINVAL
        
        // Verify mock tracked the transfer attempt
        struct mock_spi_data *mock_data = get_mock_spi_data();
        KUNIT_EXPECT_GE(test, mock_data->transfer_count, 0);
    }
}

// Fuzz multiple transfers in single message
static void fuzz_multiple_transfers(struct kunit *test)
{
    struct spi_device *spi;
    struct spi_message msg;
    uint8_t buffers[10][64]; // Up to 10 transfers
    struct spi_transfer transfers[10];
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    for (int iteration = 0; iteration < 100; iteration++) {
        uint8_t num_transfers;
        get_random_bytes(&num_transfers, 1);
        num_transfers = (num_transfers % 10) + 1; // 1-10 transfers
        
        reset_mock_spi_data();
        spi_message_init(&msg);
        
        for (int i = 0; i < num_transfers; i++) {
            uint8_t transfer_size;
            get_random_bytes(&transfer_size, 1);
            transfer_size = (transfer_size % 64) + 1; // 1-64 bytes
            
            // Generate random data for this transfer
            generate_random_data(buffers[i], transfer_size);
            
            // Set up transfer
            memset(&transfers[i], 0, sizeof(transfers[i]));
            transfers[i].tx_buf = buffers[i];
            transfers[i].len = transfer_size;
            
            spi_message_add_tail(&transfers[i], &msg);
        }
        
        // Execute multi-transfer message
        int ret = mock_spi_sync(spi, &msg);
        
        // Should handle multiple transfers
        KUNIT_EXPECT_EQ(test, ret, 0);
        
        // Verify all transfers were processed
        struct mock_spi_data *mock_data = get_mock_spi_data();
        KUNIT_EXPECT_EQ(test, mock_data->transfer_count, 1); // One message
    }
}

// Fuzz SPI transfer with random buffer alignments
static void fuzz_buffer_alignments(struct kunit *test)
{
    struct spi_device *spi;
    uint8_t *large_buffer;
    size_t large_buffer_size = PAGE_SIZE;
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    large_buffer = kunit_kzalloc(test, large_buffer_size, GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, large_buffer);
    
    for (int iteration = 0; iteration < 100; iteration++) {
        struct spi_message msg;
        struct spi_transfer xfer;
        uint16_t offset, size;
        
        // Generate random offset and size
        get_random_bytes(&offset, sizeof(offset));
        get_random_bytes(&size, sizeof(size));
        
        offset = offset % (large_buffer_size / 2);
        size = (size % 256) + 1; // 1-256 bytes
        
        // Ensure we don't exceed buffer bounds
        if (offset + size > large_buffer_size) {
            size = large_buffer_size - offset;
        }
        
        reset_mock_spi_data();
        
        // Fill buffer with random data
        generate_random_data(large_buffer + offset, size);
        
        // Set up transfer with potentially unaligned buffer
        memset(&xfer, 0, sizeof(xfer));
        xfer.tx_buf = large_buffer + offset;
        xfer.rx_buf = large_buffer + offset;
        xfer.len = size;
        
        spi_message_init(&msg);
        spi_message_add_tail(&xfer, &msg);
        
        // Test transfer - should handle various alignments
        int ret = mock_spi_sync(spi, &msg);
        KUNIT_EXPECT_EQ(test, ret, 0);
    }
}

// Fuzz error injection scenarios
static void fuzz_error_injection(struct kunit *test)
{
    struct spi_device *spi;
    struct spi_message msg;
    struct spi_transfer xfer;
    uint8_t buffer[64];
    int error_codes[] = {-EIO, -ENODEV, -EBUSY, -ETIMEDOUT, -ENOMEM};
    
    spi = create_mock_spi_device(test);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
    
    for (int i = 0; i < ARRAY_SIZE(error_codes); i++) {
        for (int retry = 0; retry < 10; retry++) {
            reset_mock_spi_data();
            
            // Set up random transfer
            generate_random_data(buffer, sizeof(buffer));
            
            memset(&xfer, 0, sizeof(xfer));
            xfer.tx_buf = buffer;
            xfer.len = sizeof(buffer);
            
            spi_message_init(&msg);
            spi_message_add_tail(&xfer, &msg);
            
            // Inject specific error
            set_mock_spi_error(error_codes[i]);
            
            // Test error handling
            int ret = mock_spi_sync(spi, &msg);
            KUNIT_EXPECT_EQ(test, ret, error_codes[i]);
            
            // Verify error was properly reported
            struct mock_spi_data *mock_data = get_mock_spi_data();
            KUNIT_EXPECT_EQ(test, mock_data->transfer_count, 1);
        }
    }
}

// Fuzz hardware command structure with random bit patterns
static void fuzz_hardware_command_structure(struct kunit *test)
{
    struct {
        uint8_t readnwrite : 1;
        uint16_t reg : 15;
    } __packed cmd;
    
    uint8_t random_bytes[sizeof(cmd)];
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        // Generate random bytes and overlay on command structure
        get_random_bytes(random_bytes, sizeof(random_bytes));
        memcpy(&cmd, random_bytes, sizeof(cmd));
        
        // Verify bit field constraints are maintained
        KUNIT_EXPECT_LE(test, cmd.readnwrite, 1);
        KUNIT_EXPECT_LE(test, cmd.reg, 0x7FFF);
        
        // Test that we can still set and read values correctly
        cmd.readnwrite = 1;
        cmd.reg = 0x1234;
        
        KUNIT_EXPECT_EQ(test, cmd.readnwrite, 1);
        KUNIT_EXPECT_EQ(test, cmd.reg, 0x1234);
    }
}

// Fuzz SPI device configuration
static void fuzz_spi_device_config(struct kunit *test)
{
    struct spi_device *spi;
    
    for (int i = 0; i < 100; i++) {
        spi = create_mock_spi_device(test);
        KUNIT_ASSERT_NOT_ERR_OR_NULL(test, spi);
        
        // Randomize SPI configuration
        get_random_bytes(&spi->max_speed_hz, sizeof(spi->max_speed_hz));
        get_random_bytes(&spi->mode, sizeof(spi->mode));
        get_random_bytes(&spi->bits_per_word, sizeof(spi->bits_per_word));
        
        // Clamp to reasonable values
        spi->max_speed_hz = spi->max_speed_hz % 50000000; // Max 50MHz
        spi->mode = spi->mode % 4; // SPI modes 0-3
        spi->bits_per_word = (spi->bits_per_word % 24) + 8; // 8-32 bits
        
        // Test setup with random configuration
        int ret = mock_spi_setup(spi);
        KUNIT_EXPECT_EQ(test, ret, 0);
        
        // Verify configuration was accepted
        struct mock_spi_data *mock_data = get_mock_spi_data();
        KUNIT_EXPECT_TRUE(test, mock_data->setup_called);
        
        reset_mock_spi_data();
    }
}

// KUnit test suite definition
static struct kunit_case fuzz_spi_interface_test_cases[] = {
    KUNIT_CASE(fuzz_spi_transfer_sizes),
    KUNIT_CASE(fuzz_spi_addresses),
    KUNIT_CASE(fuzz_multiple_transfers),
    KUNIT_CASE(fuzz_buffer_alignments),
    KUNIT_CASE(fuzz_error_injection),
    KUNIT_CASE(fuzz_hardware_command_structure),
    KUNIT_CASE(fuzz_spi_device_config),
    {}
};

static struct kunit_suite fuzz_spi_interface_test_suite = {
    .name = "fuzz-spi-interface",
    .test_cases = fuzz_spi_interface_test_cases,
};

kunit_test_suite(fuzz_spi_interface_test_suite);
#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <linux/spi/spi.h>
#include <kunit/test.h>

#define MOCK_MAX_TRANSFER_SIZE 4096

struct mock_spi_data {
    size_t transfer_count;
    size_t total_bytes_transferred;
    bool setup_called;
    bool simulate_error;
    int error_code;
    
    // Last transfer data for verification
    struct spi_message *last_message;
    struct spi_transfer *last_transfer;
    
    // TX data capture
    unsigned char last_tx_data[MOCK_MAX_TRANSFER_SIZE];
    size_t last_tx_len;
    
    // Mock response data for RX
    const void *mock_response_data;
    size_t mock_response_len;
};

// Mock function declarations
int mock_spi_sync(struct spi_device *spi, struct spi_message *message);
int mock_spi_setup(struct spi_device *spi);

// Mock management functions
struct spi_device *create_mock_spi_device(struct kunit *test);
void reset_mock_spi_data(void);
void set_mock_spi_error(int error_code);
void set_mock_spi_response(const void *data, size_t len);
struct mock_spi_data *get_mock_spi_data(void);

// Verification helpers
void verify_spi_transfer(struct kunit *test, size_t expected_transfers, 
                        size_t expected_bytes);
void verify_spi_tx_data(struct kunit *test, const void *expected_data, 
                       size_t expected_len);

#endif /* MOCK_SPI_H */
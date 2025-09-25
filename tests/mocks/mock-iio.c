// Mock IIO framework for testing Matrix Creator environmental sensors
#include <kunit/test.h>
#include <linux/iio/iio.h>
#include <linux/device.h>
#include <linux/module.h>
#include "mock-iio.h"

static struct mock_iio_data mock_data;

// Mock IIO device registration
int mock_iio_device_register(struct iio_dev *indio_dev)
{
    if (!indio_dev) {
        return -EINVAL;
    }
    
    mock_data.device_registered = true;
    mock_data.last_registered_device = indio_dev;
    mock_data.register_count++;
    
    return mock_data.simulate_register_error ? -ENOMEM : 0;
}

// Mock IIO device unregistration
void mock_iio_device_unregister(struct iio_dev *indio_dev)
{
    if (indio_dev) {
        mock_data.device_unregistered = true;
        mock_data.unregister_count++;
    }
}

// Mock IIO device allocation
struct iio_dev *mock_iio_device_alloc(struct device *parent, int sizeof_priv)
{
    struct iio_dev *indio_dev;
    
    if (mock_data.simulate_alloc_error) {
        return NULL;
    }
    
    indio_dev = kzalloc(sizeof(*indio_dev) + sizeof_priv, GFP_KERNEL);
    if (!indio_dev) {
        return NULL;
    }
    
    indio_dev->dev.parent = parent;
    indio_dev->priv = (char *)indio_dev + sizeof(*indio_dev);
    
    mock_data.device_allocated = true;
    mock_data.alloc_count++;
    mock_data.last_allocated_device = indio_dev;
    
    return indio_dev;
}

// Mock IIO device deallocation
void mock_iio_device_free(struct iio_dev *indio_dev)
{
    if (indio_dev) {
        mock_data.device_freed = true;
        mock_data.free_count++;
        kfree(indio_dev);
    }
}

// Mock IIO raw value read
int mock_iio_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
                     int *val, int *val2, long mask)
{
    if (!indio_dev || !chan || !val) {
        return -EINVAL;
    }
    
    mock_data.read_raw_called = true;
    mock_data.last_read_channel = chan;
    mock_data.last_read_mask = mask;
    mock_data.read_count++;
    
    // Return mock values based on channel type and mask
    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        *val = mock_data.mock_raw_values[chan->channel];
        return IIO_VAL_INT;
    case IIO_CHAN_INFO_SCALE:
        *val = mock_data.mock_scale_val;
        *val2 = mock_data.mock_scale_val2;
        return IIO_VAL_INT_PLUS_MICRO;
    case IIO_CHAN_INFO_OFFSET:
        *val = mock_data.mock_offset;
        return IIO_VAL_INT;
    default:
        return -EINVAL;
    }
}

// Create a mock IIO device for testing
struct iio_dev *create_mock_iio_device(struct kunit *test, int sizeof_priv)
{
    struct iio_dev *indio_dev;
    
    reset_mock_iio_data();
    
    indio_dev = mock_iio_device_alloc(NULL, sizeof_priv);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    // Set up basic device properties
    indio_dev->name = "mock_iio_device";
    indio_dev->modes = INDIO_DIRECT_MODE;
    
    return indio_dev;
}

// Reset mock data for clean test state
void reset_mock_iio_data(void)
{
    memset(&mock_data, 0, sizeof(mock_data));
    
    // Set some reasonable default mock values
    mock_data.mock_scale_val = 1;
    mock_data.mock_scale_val2 = 1000000; // 1.0 in IIO_VAL_INT_PLUS_MICRO format
    mock_data.mock_offset = 0;
    
    // Default raw values for different channels
    for (int i = 0; i < MOCK_MAX_IIO_CHANNELS; i++) {
        mock_data.mock_raw_values[i] = 1000 + i * 100;
    }
}

// Configure mock to simulate errors
void set_mock_iio_alloc_error(bool simulate)
{
    mock_data.simulate_alloc_error = simulate;
}

void set_mock_iio_register_error(bool simulate)
{
    mock_data.simulate_register_error = simulate;
}

// Set mock sensor values
void set_mock_iio_raw_value(int channel, int value)
{
    if (channel >= 0 && channel < MOCK_MAX_IIO_CHANNELS) {
        mock_data.mock_raw_values[channel] = value;
    }
}

void set_mock_iio_scale(int val, int val2)
{
    mock_data.mock_scale_val = val;
    mock_data.mock_scale_val2 = val2;
}

void set_mock_iio_offset(int offset)
{
    mock_data.mock_offset = offset;
}

// Get mock data for verification in tests
struct mock_iio_data *get_mock_iio_data(void)
{
    return &mock_data;
}

// Verification helpers
void verify_iio_device_lifecycle(struct kunit *test, bool should_be_registered)
{
    KUNIT_EXPECT_TRUE(test, mock_data.device_allocated);
    KUNIT_EXPECT_EQ(test, mock_data.device_registered, should_be_registered);
    
    if (should_be_registered) {
        KUNIT_EXPECT_GT(test, mock_data.register_count, 0);
    }
}

void verify_iio_read_operations(struct kunit *test, int expected_reads)
{
    KUNIT_EXPECT_EQ(test, mock_data.read_count, expected_reads);
    if (expected_reads > 0) {
        KUNIT_EXPECT_TRUE(test, mock_data.read_raw_called);
    }
}
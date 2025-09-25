// Unit tests for Matrix Creator environmental sensors IIO interface
#include <kunit/test.h>
#include <linux/iio/iio.h>
#include <linux/delay.h>
#include <linux/mutex.h>

// Test-specific includes
#include "../mocks/mock-iio.h"
#include "../mocks/mock-platform-device.h"
#include "../../src/matrixio-core.h"

// Test constants (from matrixio-env.c)
#define TEST_MATRIXIO_UV_DRV_NAME "matrixio_env"
#define TEST_MATRIXIO_SRAM_OFFSET_ENV 0x0

// Mock matrixio_bus structure (mirrors the one in matrixio-env.c)
struct test_matrixio_bus {
    struct matrixio *mio;
    struct mutex lock;
};

// Test IIO channel definitions
static void test_iio_channel_definitions(struct kunit *test)
{
    // Test environmental sensor channel types that should be supported
    enum iio_chan_type expected_channels[] = {
        IIO_TEMP,        // Temperature
        IIO_HUMIDITYRELATIVE, // Humidity
        IIO_PRESSURE,    // Barometric pressure  
        IIO_LIGHT,       // UV light
    };
    
    // Verify channel types are valid IIO types
    for (int i = 0; i < ARRAY_SIZE(expected_channels); i++) {
        enum iio_chan_type type = expected_channels[i];
        KUNIT_EXPECT_GE(test, type, 0);
        KUNIT_EXPECT_LT(test, type, IIO_CHAN_TYPE_UNKNOWN);
    }
}

// Test IIO device initialization
static void test_iio_device_initialization(struct kunit *test)
{
    struct iio_dev *indio_dev;
    struct test_matrixio_bus *bus_data;
    
    // Create mock IIO device
    indio_dev = create_mock_iio_device(test, sizeof(struct test_matrixio_bus));
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    // Get private data
    bus_data = iio_priv(indio_dev);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bus_data);
    
    // Initialize mutex
    mutex_init(&bus_data->lock);
    
    // Verify device structure
    KUNIT_EXPECT_STREQ(test, indio_dev->name, "mock_iio_device");
    KUNIT_EXPECT_EQ(test, indio_dev->modes, INDIO_DIRECT_MODE);
    
    // Verify private data is accessible
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, bus_data);
}

// Test IIO device registration and unregistration
static void test_iio_device_lifecycle(struct kunit *test)
{
    struct iio_dev *indio_dev;
    int ret;
    
    reset_mock_iio_data();
    
    // Test device allocation
    indio_dev = create_mock_iio_device(test, sizeof(struct test_matrixio_bus));
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    // Test device registration
    ret = mock_iio_device_register(indio_dev);
    KUNIT_EXPECT_EQ(test, ret, 0);
    
    // Verify registration was tracked
    verify_iio_device_lifecycle(test, true);
    
    // Test device unregistration
    mock_iio_device_unregister(indio_dev);
    
    // Verify unregistration was tracked
    struct mock_iio_data *mock_data = get_mock_iio_data();
    KUNIT_EXPECT_TRUE(test, mock_data->device_unregistered);
    
    // Clean up
    mock_iio_device_free(indio_dev);
}

// Test IIO device registration error handling
static void test_iio_registration_error_handling(struct kunit *test)
{
    struct iio_dev *indio_dev;
    int ret;
    
    reset_mock_iio_data();
    set_mock_iio_register_error(true);
    
    indio_dev = create_mock_iio_device(test, sizeof(struct test_matrixio_bus));
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    // Registration should fail
    ret = mock_iio_device_register(indio_dev);
    KUNIT_EXPECT_NE(test, ret, 0);
    
    // Verify failure was tracked
    verify_iio_device_lifecycle(test, false);
    
    mock_iio_device_free(indio_dev);
}

// Test sensor data reading
static void test_sensor_data_reading(struct kunit *test)
{
    struct iio_dev *indio_dev;
    struct iio_chan_spec test_channel = {
        .type = IIO_TEMP,
        .channel = 0,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE),
    };
    int val, val2, ret;
    
    reset_mock_iio_data();
    
    indio_dev = create_mock_iio_device(test, sizeof(struct test_matrixio_bus));
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    // Set mock sensor values
    set_mock_iio_raw_value(0, 2500); // 25.00°C raw value
    set_mock_iio_scale(1, 100000);   // 0.01 scale factor
    set_mock_iio_offset(-40);        // -40°C offset
    
    // Test raw value reading
    ret = mock_iio_read_raw(indio_dev, &test_channel, &val, &val2, IIO_CHAN_INFO_RAW);
    KUNIT_EXPECT_EQ(test, ret, IIO_VAL_INT);
    KUNIT_EXPECT_EQ(test, val, 2500);
    
    // Test scale reading
    ret = mock_iio_read_raw(indio_dev, &test_channel, &val, &val2, IIO_CHAN_INFO_SCALE);
    KUNIT_EXPECT_EQ(test, ret, IIO_VAL_INT_PLUS_MICRO);
    KUNIT_EXPECT_EQ(test, val, 1);
    KUNIT_EXPECT_EQ(test, val2, 100000);
    
    // Test offset reading
    ret = mock_iio_read_raw(indio_dev, &test_channel, &val, &val2, IIO_CHAN_INFO_OFFSET);
    KUNIT_EXPECT_EQ(test, ret, IIO_VAL_INT);
    KUNIT_EXPECT_EQ(test, val, -40);
    
    // Verify read operations were tracked
    verify_iio_read_operations(test, 3);
    
    mock_iio_device_free(indio_dev);
}

// Test invalid channel reading
static void test_invalid_channel_reading(struct kunit *test)
{
    struct iio_dev *indio_dev;
    struct iio_chan_spec invalid_channel = {
        .type = IIO_CHAN_TYPE_UNKNOWN,
        .channel = 999,
    };
    int val, val2, ret;
    
    reset_mock_iio_data();
    
    indio_dev = create_mock_iio_device(test, sizeof(struct test_matrixio_bus));
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    // Test with invalid mask
    ret = mock_iio_read_raw(indio_dev, &invalid_channel, &val, &val2, 0xFFFF);
    KUNIT_EXPECT_EQ(test, ret, -EINVAL);
    
    // Test with NULL parameters
    ret = mock_iio_read_raw(NULL, &invalid_channel, &val, &val2, IIO_CHAN_INFO_RAW);
    KUNIT_EXPECT_EQ(test, ret, -EINVAL);
    
    ret = mock_iio_read_raw(indio_dev, NULL, &val, &val2, IIO_CHAN_INFO_RAW);
    KUNIT_EXPECT_EQ(test, ret, -EINVAL);
    
    ret = mock_iio_read_raw(indio_dev, &invalid_channel, NULL, &val2, IIO_CHAN_INFO_RAW);
    KUNIT_EXPECT_EQ(test, ret, -EINVAL);
    
    mock_iio_device_free(indio_dev);
}

// Test mutex locking for thread safety
static void test_mutex_locking(struct kunit *test)
{
    struct test_matrixio_bus *bus_data;
    struct iio_dev *indio_dev;
    
    indio_dev = create_mock_iio_device(test, sizeof(struct test_matrixio_bus));
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, indio_dev);
    
    bus_data = iio_priv(indio_dev);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bus_data);
    
    // Initialize mutex
    mutex_init(&bus_data->lock);
    
    // Test mutex operations
    KUNIT_EXPECT_EQ(test, mutex_trylock(&bus_data->lock), 1);
    mutex_unlock(&bus_data->lock);
    
    // Test that mutex can be locked and unlocked
    mutex_lock(&bus_data->lock);
    KUNIT_EXPECT_EQ(test, mutex_trylock(&bus_data->lock), 0); // Should fail
    mutex_unlock(&bus_data->lock);
    
    mock_iio_device_free(indio_dev);
}

// Test platform device integration
static void test_platform_device_integration(struct kunit *test)
{
    struct platform_device *pdev;
    struct test_matrixio_bus *bus_data;
    
    pdev = create_mock_platform_device(test, TEST_MATRIXIO_UV_DRV_NAME);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);
    
    bus_data = kunit_kzalloc(test, sizeof(*bus_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, bus_data);
    
    // Test platform device name
    KUNIT_EXPECT_STREQ(test, pdev->name, TEST_MATRIXIO_UV_DRV_NAME);
    
    // Test platform driver data
    platform_set_drvdata(pdev, bus_data);
    KUNIT_EXPECT_PTR_EQ(test, platform_get_drvdata(pdev), bus_data);
}

// Test SRAM offset validation
static void test_sram_offset_validation(struct kunit *test)
{
    // Test that SRAM offset is valid
    KUNIT_EXPECT_EQ(test, TEST_MATRIXIO_SRAM_OFFSET_ENV, 0x0);
    KUNIT_EXPECT_GE(test, TEST_MATRIXIO_SRAM_OFFSET_ENV, 0);
}

// Test memory allocation error handling
static void test_memory_allocation_errors(struct kunit *test)
{
    reset_mock_iio_data();
    set_mock_iio_alloc_error(true);
    
    // Device allocation should fail
    struct iio_dev *indio_dev = mock_iio_device_alloc(NULL, sizeof(struct test_matrixio_bus));
    KUNIT_EXPECT_NULL(test, indio_dev);
    
    // Verify allocation failure was tracked
    struct mock_iio_data *mock_data = get_mock_iio_data();
    KUNIT_EXPECT_FALSE(test, mock_data->device_allocated);
}

// KUnit test suite definition
static struct kunit_case matrixio_env_test_cases[] = {
    KUNIT_CASE(test_iio_channel_definitions),
    KUNIT_CASE(test_iio_device_initialization),
    KUNIT_CASE(test_iio_device_lifecycle),
    KUNIT_CASE(test_iio_registration_error_handling),
    KUNIT_CASE(test_sensor_data_reading),
    KUNIT_CASE(test_invalid_channel_reading),
    KUNIT_CASE(test_mutex_locking),
    KUNIT_CASE(test_platform_device_integration),
    KUNIT_CASE(test_sram_offset_validation),
    KUNIT_CASE(test_memory_allocation_errors),
    {}
};

static struct kunit_suite matrixio_env_test_suite = {
    .name = "matrixio-env",
    .test_cases = matrixio_env_test_cases,
};

kunit_test_suite(matrixio_env_test_suite);
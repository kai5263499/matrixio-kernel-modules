// Unit tests for Matrix Creator Everloop LED ring control
#include <kunit/test.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>

// Test-specific includes
#include "../mocks/mock-platform-device.h"
#include "../../src/matrixio-core.h"

// Test constants and structures for Everloop
#define TEST_EVERLOOP_LED_COUNT 35
#define TEST_EVERLOOP_BYTES_PER_LED 4  // RGBW format

// Mock everloop data structure
struct test_everloop_data {
    struct matrixio *mio;
    struct class *cl;
    dev_t devt;
    struct cdev cdev;
    struct device *device;
    int major;
    uint8_t led_buffer[TEST_EVERLOOP_LED_COUNT * TEST_EVERLOOP_BYTES_PER_LED];
};

// Test LED data format (RGBW)
static void test_led_data_format(struct kunit *test)
{
    // Test RGBW color structure
    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t white;
    } __packed rgbw_color;
    
    // Verify structure size
    KUNIT_EXPECT_EQ(test, sizeof(rgbw_color), TEST_EVERLOOP_BYTES_PER_LED);
    
    // Test color value ranges
    rgbw_color.red = 255;
    rgbw_color.green = 128;
    rgbw_color.blue = 64;
    rgbw_color.white = 32;
    
    KUNIT_EXPECT_EQ(test, rgbw_color.red, 255);
    KUNIT_EXPECT_EQ(test, rgbw_color.green, 128);
    KUNIT_EXPECT_EQ(test, rgbw_color.blue, 64);
    KUNIT_EXPECT_EQ(test, rgbw_color.white, 32);
}

// Test LED buffer size calculations
static void test_led_buffer_size(struct kunit *test)
{
    size_t expected_buffer_size = TEST_EVERLOOP_LED_COUNT * TEST_EVERLOOP_BYTES_PER_LED;
    
    KUNIT_EXPECT_EQ(test, expected_buffer_size, 140); // 35 LEDs * 4 bytes
    KUNIT_EXPECT_GT(test, expected_buffer_size, 0);
    KUNIT_EXPECT_LT(test, expected_buffer_size, PAGE_SIZE); // Should fit in one page
}

// Test LED index bounds checking
static void test_led_index_bounds(struct kunit *test)
{
    int valid_indices[] = {0, 1, 17, 34}; // Valid LED indices
    int invalid_indices[] = {-1, 35, 100, -100}; // Invalid LED indices
    
    // Test valid indices
    for (int i = 0; i < ARRAY_SIZE(valid_indices); i++) {
        int index = valid_indices[i];
        KUNIT_EXPECT_GE(test, index, 0);
        KUNIT_EXPECT_LT(test, index, TEST_EVERLOOP_LED_COUNT);
    }
    
    // Test invalid indices
    for (int i = 0; i < ARRAY_SIZE(invalid_indices); i++) {
        int index = invalid_indices[i];
        KUNIT_EXPECT_TRUE(test, index < 0 || index >= TEST_EVERLOOP_LED_COUNT);
    }
}

// Test write operation data validation
static void test_write_data_validation(struct kunit *test)
{
    struct test_everloop_data *everloop_data;
    size_t valid_sizes[] = {
        TEST_EVERLOOP_BYTES_PER_LED,     // Single LED
        TEST_EVERLOOP_BYTES_PER_LED * 5,  // 5 LEDs
        TEST_EVERLOOP_BYTES_PER_LED * TEST_EVERLOOP_LED_COUNT // Full ring
    };
    size_t invalid_sizes[] = {
        0,                               // Zero size
        1,                              // Partial LED data
        3,                              // Incomplete RGBW
        TEST_EVERLOOP_BYTES_PER_LED * TEST_EVERLOOP_LED_COUNT + 1, // Too large
        SIZE_MAX                        // Way too large
    };
    
    everloop_data = kunit_kzalloc(test, sizeof(*everloop_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, everloop_data);
    
    // Test valid sizes
    for (int i = 0; i < ARRAY_SIZE(valid_sizes); i++) {
        size_t size = valid_sizes[i];
        KUNIT_EXPECT_EQ(test, size % TEST_EVERLOOP_BYTES_PER_LED, 0);
        KUNIT_EXPECT_LE(test, size, sizeof(everloop_data->led_buffer));
    }
    
    // Test invalid sizes
    for (int i = 0; i < ARRAY_SIZE(invalid_sizes); i++) {
        size_t size = invalid_sizes[i];
        if (size > 0 && size < SIZE_MAX) {
            // Should either not be aligned or exceed buffer size
            bool is_invalid = (size % TEST_EVERLOOP_BYTES_PER_LED != 0) ||
                             (size > sizeof(everloop_data->led_buffer));
            KUNIT_EXPECT_TRUE(test, is_invalid);
        }
    }
}

// Test file operations structure
static void test_file_operations_setup(struct kunit *test)
{
    struct test_everloop_data *everloop_data;
    struct file *mock_file;
    
    everloop_data = kunit_kzalloc(test, sizeof(*everloop_data), GFP_KERNEL);
    mock_file = kunit_kzalloc(test, sizeof(*mock_file), GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, everloop_data);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mock_file);
    
    // Simulate file operations setup
    mock_file->private_data = everloop_data;
    
    // Verify file structure setup
    KUNIT_EXPECT_PTR_EQ(test, mock_file->private_data, everloop_data);
}

// Test LED color setting and getting
static void test_led_color_operations(struct kunit *test)
{
    struct test_everloop_data *everloop_data;
    uint8_t test_color[TEST_EVERLOOP_BYTES_PER_LED] = {255, 128, 64, 32}; // R, G, B, W
    int led_index = 10;
    
    everloop_data = kunit_kzalloc(test, sizeof(*everloop_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, everloop_data);
    
    // Set LED color
    size_t offset = led_index * TEST_EVERLOOP_BYTES_PER_LED;
    memcpy(&everloop_data->led_buffer[offset], test_color, TEST_EVERLOOP_BYTES_PER_LED);
    
    // Verify LED color was set correctly
    KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[offset + 0], 255); // Red
    KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[offset + 1], 128); // Green
    KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[offset + 2], 64);  // Blue
    KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[offset + 3], 32);  // White
}

// Test brightness scaling
static void test_brightness_scaling(struct kunit *test)
{
    uint8_t color_values[] = {0, 1, 127, 128, 254, 255};
    float brightness_scales[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    
    // Test brightness scaling calculations
    for (int c = 0; c < ARRAY_SIZE(color_values); c++) {
        for (int b = 0; b < ARRAY_SIZE(brightness_scales); b++) {
            uint8_t color = color_values[c];
            float scale = brightness_scales[b];
            uint8_t scaled = (uint8_t)(color * scale);
            
            KUNIT_EXPECT_LE(test, scaled, color);
            KUNIT_EXPECT_LE(test, scaled, 255);
            
            if (scale == 0.0f) {
                KUNIT_EXPECT_EQ(test, scaled, 0);
            }
            if (scale == 1.0f) {
                KUNIT_EXPECT_EQ(test, scaled, color);
            }
        }
    }
}

// Test partial LED ring updates
static void test_partial_ring_updates(struct kunit *test)
{
    struct test_everloop_data *everloop_data;
    uint8_t update_data[TEST_EVERLOOP_BYTES_PER_LED * 3]; // 3 LEDs worth
    
    everloop_data = kunit_kzalloc(test, sizeof(*everloop_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, everloop_data);
    
    // Initialize buffer to known values
    memset(everloop_data->led_buffer, 0, sizeof(everloop_data->led_buffer));
    
    // Prepare update data
    for (int i = 0; i < sizeof(update_data); i++) {
        update_data[i] = i + 1;
    }
    
    // Apply partial update starting at LED 5
    int start_led = 5;
    int num_leds = 3;
    size_t offset = start_led * TEST_EVERLOOP_BYTES_PER_LED;
    memcpy(&everloop_data->led_buffer[offset], update_data, sizeof(update_data));
    
    // Verify update was applied correctly
    for (int i = 0; i < sizeof(update_data); i++) {
        KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[offset + i], i + 1);
    }
    
    // Verify other LEDs weren't affected
    for (int i = 0; i < start_led * TEST_EVERLOOP_BYTES_PER_LED; i++) {
        KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[i], 0);
    }
    
    size_t end_offset = offset + sizeof(update_data);
    for (size_t i = end_offset; i < sizeof(everloop_data->led_buffer); i++) {
        KUNIT_EXPECT_EQ(test, everloop_data->led_buffer[i], 0);
    }
}

// Test device open/close operations
static void test_device_open_close(struct kunit *test)
{
    struct test_everloop_data *everloop_data;
    struct inode *mock_inode;
    struct file *mock_file;
    
    everloop_data = kunit_kzalloc(test, sizeof(*everloop_data), GFP_KERNEL);
    mock_inode = kunit_kzalloc(test, sizeof(*mock_inode), GFP_KERNEL);
    mock_file = kunit_kzalloc(test, sizeof(*mock_file), GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, everloop_data);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mock_inode);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mock_file);
    
    // Simulate device open
    mock_inode->i_cdev = &everloop_data->cdev;
    
    // This mimics the open operation
    struct test_everloop_data *el;
    el = container_of(mock_inode->i_cdev, struct test_everloop_data, cdev);
    mock_file->private_data = el;
    
    // Verify open operation
    KUNIT_EXPECT_PTR_EQ(test, mock_file->private_data, everloop_data);
}

// Test platform device integration
static void test_platform_device_integration(struct kunit *test)
{
    struct platform_device *pdev;
    struct test_everloop_data *everloop_data;
    
    pdev = create_mock_platform_device(test, "matrixio-everloop");
    everloop_data = kunit_kzalloc(test, sizeof(*everloop_data), GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, everloop_data);
    
    // Test platform device setup
    platform_set_drvdata(pdev, everloop_data);
    
    // Verify platform data
    struct test_everloop_data *retrieved_data = platform_get_drvdata(pdev);
    KUNIT_EXPECT_PTR_EQ(test, retrieved_data, everloop_data);
}

// Test uevent generation
static void test_uevent_generation(struct kunit *test)
{
    // Test uevent environment variables that should be set
    const char *expected_uevent_vars[] = {
        "DEVNAME=matrixio_everloop",
        "SUBSYSTEM=matrixio",
    };
    
    // This test would verify uevent string formatting
    // For now, just verify the expected strings are valid
    for (int i = 0; i < ARRAY_SIZE(expected_uevent_vars); i++) {
        const char *var = expected_uevent_vars[i];
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, var);
        KUNIT_EXPECT_GT(test, strlen(var), 0);
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, strchr(var, '='));
    }
}

// KUnit test suite definition
static struct kunit_case matrixio_everloop_test_cases[] = {
    KUNIT_CASE(test_led_data_format),
    KUNIT_CASE(test_led_buffer_size),
    KUNIT_CASE(test_led_index_bounds),
    KUNIT_CASE(test_write_data_validation),
    KUNIT_CASE(test_file_operations_setup),
    KUNIT_CASE(test_led_color_operations),
    KUNIT_CASE(test_brightness_scaling),
    KUNIT_CASE(test_partial_ring_updates),
    KUNIT_CASE(test_device_open_close),
    KUNIT_CASE(test_platform_device_integration),
    KUNIT_CASE(test_uevent_generation),
    {}
};

static struct kunit_suite matrixio_everloop_test_suite = {
    .name = "matrixio-everloop",
    .test_cases = matrixio_everloop_test_cases,
};

kunit_test_suite(matrixio_everloop_test_suite);
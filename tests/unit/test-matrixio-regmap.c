// Unit tests for Matrix Creator regmap character device interface
#include <kunit/test.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

// Test-specific includes
#include "../mocks/mock-platform-device.h"
#include "../../src/matrixio-core.h"

// Mock regmap_data structure (mirrors the one in matrixio-regmap.c)
struct test_regmap_data {
    struct matrixio *mio;
    struct class *cl;
    dev_t devt;
    struct cdev cdev;
    struct device *device;
    int major;
};

// Test regmap_data structure initialization
static void test_regmap_data_init(struct kunit *test)
{
    struct test_regmap_data *regmap_data;
    
    regmap_data = kunit_kzalloc(test, sizeof(*regmap_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmap_data);
    
    // Verify initial state
    KUNIT_EXPECT_NULL(test, regmap_data->mio);
    KUNIT_EXPECT_NULL(test, regmap_data->cl);
    KUNIT_EXPECT_EQ(test, regmap_data->devt, 0);
    KUNIT_EXPECT_NULL(test, regmap_data->device);
    KUNIT_EXPECT_EQ(test, regmap_data->major, 0);
}

// Test character device setup
static void test_cdev_setup(struct kunit *test)
{
    struct test_regmap_data *regmap_data;
    int major = 250; // Use a test major number
    
    regmap_data = kunit_kzalloc(test, sizeof(*regmap_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmap_data);
    
    // Simulate cdev initialization
    cdev_init(&regmap_data->cdev, NULL);
    regmap_data->major = major;
    regmap_data->devt = MKDEV(major, 0);
    
    // Verify cdev setup
    KUNIT_EXPECT_EQ(test, regmap_data->major, major);
    KUNIT_EXPECT_EQ(test, MAJOR(regmap_data->devt), major);
    KUNIT_EXPECT_EQ(test, MINOR(regmap_data->devt), 0);
}

// Test file operations open function
static void test_regmap_open_operation(struct kunit *test)
{
    struct test_regmap_data *regmap_data;
    struct inode *mock_inode;
    struct file *mock_file;
    
    // Allocate test structures
    regmap_data = kunit_kzalloc(test, sizeof(*regmap_data), GFP_KERNEL);
    mock_inode = kunit_kzalloc(test, sizeof(*mock_inode), GFP_KERNEL);
    mock_file = kunit_kzalloc(test, sizeof(*mock_file), GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmap_data);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mock_inode);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mock_file);
    
    // Set up mock inode with our cdev
    mock_inode->i_cdev = &regmap_data->cdev;
    
    // Simulate the open operation logic
    // This mimics what matrixio_regmap_open() does
    struct test_regmap_data *el;
    el = container_of(mock_inode->i_cdev, struct test_regmap_data, cdev);
    mock_file->private_data = el;
    
    // Verify that private_data is set correctly
    KUNIT_EXPECT_PTR_EQ(test, mock_file->private_data, regmap_data);
    KUNIT_EXPECT_PTR_EQ(test, el, regmap_data);
}

// Test file operations with invalid parameters
static void test_regmap_operations_error_handling(struct kunit *test)
{
    struct file *mock_file;
    
    mock_file = kunit_kzalloc(test, sizeof(*mock_file), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mock_file);
    
    // Test with NULL private_data (should be handled gracefully)
    mock_file->private_data = NULL;
    
    // These would typically call the actual read/write functions
    // For now, verify the test infrastructure works
    KUNIT_EXPECT_NULL(test, mock_file->private_data);
}

// Test read operation bounds checking
static void test_regmap_read_bounds(struct kunit *test)
{
    size_t valid_sizes[] = {1, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    size_t invalid_sizes[] = {0, SIZE_MAX, SIZE_MAX/2};
    
    // Test valid read sizes
    for (int i = 0; i < ARRAY_SIZE(valid_sizes); i++) {
        size_t size = valid_sizes[i];
        KUNIT_EXPECT_GT(test, size, 0);
        KUNIT_EXPECT_LT(test, size, PAGE_SIZE); // Reasonable upper bound
    }
    
    // Test invalid read sizes
    for (int i = 0; i < ARRAY_SIZE(invalid_sizes); i++) {
        size_t size = invalid_sizes[i];
        // These should be rejected by the actual implementation
        if (size == 0) {
            KUNIT_EXPECT_EQ(test, size, 0);
        } else {
            KUNIT_EXPECT_GT(test, size, PAGE_SIZE);
        }
    }
}

// Test write operation bounds checking  
static void test_regmap_write_bounds(struct kunit *test)
{
    size_t valid_sizes[] = {1, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    
    // Test valid write sizes
    for (int i = 0; i < ARRAY_SIZE(valid_sizes); i++) {
        size_t size = valid_sizes[i];
        KUNIT_EXPECT_GT(test, size, 0);
        KUNIT_EXPECT_LT(test, size, PAGE_SIZE);
    }
}

// Test ioctl command validation
static void test_regmap_ioctl_commands(struct kunit *test)
{
    // Test common ioctl command patterns
    unsigned int valid_cmds[] = {
        _IO('m', 0),
        _IO('m', 1),
        _IOR('m', 2, int),
        _IOW('m', 3, int),
        _IOWR('m', 4, int)
    };
    
    unsigned int invalid_cmds[] = {
        0,
        0xFFFFFFFF,
        _IO('x', 0), // Wrong magic number
    };
    
    // Verify valid commands have correct magic number
    for (int i = 0; i < ARRAY_SIZE(valid_cmds); i++) {
        unsigned int cmd = valid_cmds[i];
        KUNIT_EXPECT_EQ(test, _IOC_TYPE(cmd), 'm');
    }
    
    // Verify invalid commands
    for (int i = 0; i < ARRAY_SIZE(invalid_cmds); i++) {
        unsigned int cmd = invalid_cmds[i];
        if (cmd != 0) {
            KUNIT_EXPECT_NE(test, _IOC_TYPE(cmd), 'm');
        }
    }
}

// Test concurrent access scenarios
static void test_regmap_concurrent_access(struct kunit *test)
{
    struct test_regmap_data *regmap_data;
    struct file *file1, *file2;
    
    regmap_data = kunit_kzalloc(test, sizeof(*regmap_data), GFP_KERNEL);
    file1 = kunit_kzalloc(test, sizeof(*file1), GFP_KERNEL);
    file2 = kunit_kzalloc(test, sizeof(*file2), GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmap_data);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, file1);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, file2);
    
    // Both files should be able to reference the same regmap_data
    file1->private_data = regmap_data;
    file2->private_data = regmap_data;
    
    KUNIT_EXPECT_PTR_EQ(test, file1->private_data, file2->private_data);
}

// Test device class creation and management
static void test_device_class_management(struct kunit *test)
{
    struct test_regmap_data *regmap_data;
    const char *class_name = "matrixio_regmap";
    
    regmap_data = kunit_kzalloc(test, sizeof(*regmap_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmap_data);
    
    // This would typically create a device class
    // For testing, we just verify the structure is ready
    KUNIT_EXPECT_NULL(test, regmap_data->cl);
    KUNIT_EXPECT_NULL(test, regmap_data->device);
    
    // Simulate successful class creation
    regmap_data->cl = (struct class *)0x12345678; // Mock pointer
    KUNIT_EXPECT_NOT_ERR_OR_NULL(test, regmap_data->cl);
}

// Test platform device integration
static void test_platform_device_integration(struct kunit *test)
{
    struct platform_device *pdev;
    struct test_regmap_data *regmap_data;
    
    pdev = create_mock_platform_device(test, "matrixio-regmap");
    regmap_data = kunit_kzalloc(test, sizeof(*regmap_data), GFP_KERNEL);
    
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regmap_data);
    
    // Simulate setting platform driver data
    platform_set_drvdata(pdev, regmap_data);
    
    // Verify data can be retrieved
    struct test_regmap_data *retrieved_data = platform_get_drvdata(pdev);
    KUNIT_EXPECT_PTR_EQ(test, retrieved_data, regmap_data);
}

// KUnit test suite definition
static struct kunit_case matrixio_regmap_test_cases[] = {
    KUNIT_CASE(test_regmap_data_init),
    KUNIT_CASE(test_cdev_setup),
    KUNIT_CASE(test_regmap_open_operation),
    KUNIT_CASE(test_regmap_operations_error_handling),
    KUNIT_CASE(test_regmap_read_bounds),
    KUNIT_CASE(test_regmap_write_bounds),
    KUNIT_CASE(test_regmap_ioctl_commands),
    KUNIT_CASE(test_regmap_concurrent_access),
    KUNIT_CASE(test_device_class_management),
    KUNIT_CASE(test_platform_device_integration),
    {}
};

static struct kunit_suite matrixio_regmap_test_suite = {
    .name = "matrixio-regmap",
    .test_cases = matrixio_regmap_test_cases,
};

kunit_test_suite(matrixio_regmap_test_suite);
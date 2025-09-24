// Mock platform device framework for testing Matrix Creator modules
#include <kunit/test.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include "mock-platform-device.h"

static struct mock_platform_data mock_data;

// Mock platform driver registration
int mock_platform_driver_register(struct platform_driver *drv)
{
    if (!drv) {
        return -EINVAL;
    }
    
    mock_data.driver_registered = true;
    mock_data.last_registered_driver = drv;
    mock_data.register_count++;
    
    return mock_data.simulate_register_error ? -ENODEV : 0;
}

// Mock platform driver unregistration  
void mock_platform_driver_unregister(struct platform_driver *drv)
{
    if (drv) {
        mock_data.driver_unregistered = true;
        mock_data.unregister_count++;
    }
}

// Mock platform device registration
int mock_platform_device_register(struct platform_device *pdev)
{
    if (!pdev) {
        return -EINVAL;
    }
    
    mock_data.device_registered = true;
    mock_data.last_registered_device = pdev;
    mock_data.device_register_count++;
    
    return 0;
}

// Mock platform device unregistration
void mock_platform_device_unregister(struct platform_device *pdev)
{
    if (pdev) {
        mock_data.device_unregistered = true;
        mock_data.device_unregister_count++;
    }
}

// Mock probe function
int mock_platform_probe(struct platform_device *pdev)
{
    if (!pdev) {
        return -EINVAL;
    }
    
    mock_data.probe_called = true;
    mock_data.probe_count++;
    mock_data.last_probed_device = pdev;
    
    // Simulate probe behavior
    if (mock_data.simulate_probe_error) {
        return mock_data.probe_error_code;
    }
    
    // Set up mock private data if requested
    if (mock_data.mock_private_data) {
        platform_set_drvdata(pdev, mock_data.mock_private_data);
    }
    
    return 0;
}

// Mock remove function
int mock_platform_remove(struct platform_device *pdev)
{
    if (!pdev) {
        return -EINVAL;
    }
    
    mock_data.remove_called = true;
    mock_data.remove_count++;
    mock_data.last_removed_device = pdev;
    
    return mock_data.simulate_remove_error ? -EIO : 0;
}

// Create a mock platform device for testing
struct platform_device *create_mock_platform_device(struct kunit *test, 
                                                    const char *name)
{
    struct platform_device *pdev;
    
    pdev = kunit_kzalloc(test, sizeof(*pdev), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev);
    
    // Initialize device structure
    device_initialize(&pdev->dev);
    pdev->name = name ? name : "mock_platform_device";
    pdev->id = -1;
    
    // Set up device name
    dev_set_name(&pdev->dev, "%s", pdev->name);
    
    reset_mock_platform_data();
    
    return pdev;
}

// Create a mock platform driver for testing
struct platform_driver *create_mock_platform_driver(struct kunit *test,
                                                    const char *name)
{
    struct platform_driver *drv;
    
    drv = kunit_kzalloc(test, sizeof(*drv), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drv);
    
    drv->driver.name = name ? name : "mock_platform_driver";
    drv->probe = mock_platform_probe;
    drv->remove = mock_platform_remove;
    
    return drv;
}

// Reset mock data for clean test state
void reset_mock_platform_data(void)
{
    memset(&mock_data, 0, sizeof(mock_data));
}

// Configure mock to simulate errors
void set_mock_platform_register_error(bool simulate)
{
    mock_data.simulate_register_error = simulate;
}

void set_mock_platform_probe_error(int error_code)
{
    mock_data.simulate_probe_error = true;
    mock_data.probe_error_code = error_code;
}

void set_mock_platform_remove_error(bool simulate)
{
    mock_data.simulate_remove_error = simulate;
}

// Set mock private data for device
void set_mock_platform_private_data(void *data)
{
    mock_data.mock_private_data = data;
}

// Get mock data for verification in tests
struct mock_platform_data *get_mock_platform_data(void)
{
    return &mock_data;
}

// Verification helpers
void verify_platform_driver_lifecycle(struct kunit *test, bool should_be_registered)
{
    KUNIT_EXPECT_EQ(test, mock_data.driver_registered, should_be_registered);
    
    if (should_be_registered) {
        KUNIT_EXPECT_GT(test, mock_data.register_count, 0);
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mock_data.last_registered_driver);
    }
}

void verify_platform_device_lifecycle(struct kunit *test, bool should_be_registered)
{
    KUNIT_EXPECT_EQ(test, mock_data.device_registered, should_be_registered);
    
    if (should_be_registered) {
        KUNIT_EXPECT_GT(test, mock_data.device_register_count, 0);
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, mock_data.last_registered_device);
    }
}

void verify_platform_probe_remove(struct kunit *test, int expected_probes, 
                                  int expected_removes)
{
    KUNIT_EXPECT_EQ(test, mock_data.probe_count, expected_probes);
    KUNIT_EXPECT_EQ(test, mock_data.remove_count, expected_removes);
    
    if (expected_probes > 0) {
        KUNIT_EXPECT_TRUE(test, mock_data.probe_called);
    }
    
    if (expected_removes > 0) {
        KUNIT_EXPECT_TRUE(test, mock_data.remove_called);
    }
}
#ifndef MOCK_PLATFORM_DEVICE_H
#define MOCK_PLATFORM_DEVICE_H

#include <linux/platform_device.h>
#include <kunit/test.h>

struct mock_platform_data {
    // Driver lifecycle tracking
    bool driver_registered;
    bool driver_unregistered;
    int register_count;
    int unregister_count;
    
    // Device lifecycle tracking
    bool device_registered;
    bool device_unregistered;
    int device_register_count;
    int device_unregister_count;
    
    // Probe/remove tracking
    bool probe_called;
    bool remove_called;
    int probe_count;
    int remove_count;
    
    // Error simulation
    bool simulate_register_error;
    bool simulate_probe_error;
    bool simulate_remove_error;
    int probe_error_code;
    
    // Mock data
    void *mock_private_data;
    
    // Last operated objects
    struct platform_driver *last_registered_driver;
    struct platform_device *last_registered_device;
    struct platform_device *last_probed_device;
    struct platform_device *last_removed_device;
};

// Mock function declarations
int mock_platform_driver_register(struct platform_driver *drv);
void mock_platform_driver_unregister(struct platform_driver *drv);
int mock_platform_device_register(struct platform_device *pdev);
void mock_platform_device_unregister(struct platform_device *pdev);
int mock_platform_probe(struct platform_device *pdev);
int mock_platform_remove(struct platform_device *pdev);

// Mock management functions
struct platform_device *create_mock_platform_device(struct kunit *test, 
                                                    const char *name);
struct platform_driver *create_mock_platform_driver(struct kunit *test,
                                                    const char *name);
void reset_mock_platform_data(void);
void set_mock_platform_register_error(bool simulate);
void set_mock_platform_probe_error(int error_code);
void set_mock_platform_remove_error(bool simulate);
void set_mock_platform_private_data(void *data);
struct mock_platform_data *get_mock_platform_data(void);

// Verification helpers
void verify_platform_driver_lifecycle(struct kunit *test, bool should_be_registered);
void verify_platform_device_lifecycle(struct kunit *test, bool should_be_registered);
void verify_platform_probe_remove(struct kunit *test, int expected_probes, 
                                  int expected_removes);

#endif /* MOCK_PLATFORM_DEVICE_H */
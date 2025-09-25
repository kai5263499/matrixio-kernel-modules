#ifndef MOCK_IIO_H
#define MOCK_IIO_H

#include <linux/iio/iio.h>
#include <kunit/test.h>

#define MOCK_MAX_IIO_CHANNELS 16

struct mock_iio_data {
    // Device lifecycle tracking
    bool device_allocated;
    bool device_freed;
    bool device_registered;
    bool device_unregistered;
    int alloc_count;
    int free_count;
    int register_count;
    int unregister_count;
    
    // Error simulation
    bool simulate_alloc_error;
    bool simulate_register_error;
    
    // Read operation tracking
    bool read_raw_called;
    int read_count;
    const struct iio_chan_spec *last_read_channel;
    long last_read_mask;
    
    // Mock sensor values
    int mock_raw_values[MOCK_MAX_IIO_CHANNELS];
    int mock_scale_val;
    int mock_scale_val2;
    int mock_offset;
    
    // Last operated devices
    struct iio_dev *last_allocated_device;
    struct iio_dev *last_registered_device;
};

// Mock function declarations
int mock_iio_device_register(struct iio_dev *indio_dev);
void mock_iio_device_unregister(struct iio_dev *indio_dev);
struct iio_dev *mock_iio_device_alloc(struct device *parent, int sizeof_priv);
void mock_iio_device_free(struct iio_dev *indio_dev);
int mock_iio_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan,
                     int *val, int *val2, long mask);

// Mock management functions
struct iio_dev *create_mock_iio_device(struct kunit *test, int sizeof_priv);
void reset_mock_iio_data(void);
void set_mock_iio_alloc_error(bool simulate);
void set_mock_iio_register_error(bool simulate);
void set_mock_iio_raw_value(int channel, int value);
void set_mock_iio_scale(int val, int val2);
void set_mock_iio_offset(int offset);
struct mock_iio_data *get_mock_iio_data(void);

// Verification helpers
void verify_iio_device_lifecycle(struct kunit *test, bool should_be_registered);
void verify_iio_read_operations(struct kunit *test, int expected_reads);

#endif /* MOCK_IIO_H */
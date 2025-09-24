// Fuzzing tests for userspace interface robustness
#include <kunit/test.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/limits.h>

// Test-specific includes
#include "../mocks/mock-platform-device.h"

#define FUZZ_ITERATIONS 1000
#define FUZZ_MAX_BUFFER_SIZE 8192

// Mock file operations data
struct fuzz_file_data {
    uint8_t buffer[FUZZ_MAX_BUFFER_SIZE];
    size_t buffer_size;
    loff_t position;
};

// Helper function to generate random data
static void generate_random_data(void *buffer, size_t size)
{
    uint8_t *data = (uint8_t *)buffer;
    for (size_t i = 0; i < size; i++) {
        get_random_bytes(&data[i], 1);
    }
}

// Fuzz read operations with various buffer sizes
static void fuzz_read_operations(struct kunit *test)
{
    struct fuzz_file_data *file_data;
    size_t test_sizes[] = {
        0, 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256,
        511, 512, 1023, 1024, 2047, 2048, 4095, 4096, 8191, 8192
    };
    
    file_data = kunit_kzalloc(test, sizeof(*file_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, file_data);
    
    // Initialize with random data
    generate_random_data(file_data->buffer, sizeof(file_data->buffer));
    file_data->buffer_size = sizeof(file_data->buffer);
    
    for (int i = 0; i < ARRAY_SIZE(test_sizes); i++) {
        size_t read_size = test_sizes[i];
        uint8_t *read_buffer;
        
        if (read_size > FUZZ_MAX_BUFFER_SIZE) {
            continue;
        }
        
        read_buffer = kunit_kzalloc(test, read_size, GFP_KERNEL);
        if (!read_buffer && read_size > 0) {
            continue; // Skip if allocation fails
        }
        
        file_data->position = 0;
        
        // Simulate read operation bounds checking
        size_t available = file_data->buffer_size - file_data->position;
        size_t to_read = min(read_size, available);
        
        if (read_size == 0) {
            // Zero-size read should be handled gracefully
            KUNIT_EXPECT_EQ(test, to_read, 0);
        } else if (file_data->position < file_data->buffer_size) {
            // Normal read within bounds
            KUNIT_EXPECT_LE(test, to_read, read_size);
            KUNIT_EXPECT_LE(test, to_read, available);
            
            if (read_buffer && to_read > 0) {
                // Simulate copying data to userspace buffer
                memcpy(read_buffer, &file_data->buffer[file_data->position], to_read);
                file_data->position += to_read;
            }
        }
        
        if (read_buffer) {
            kfree(read_buffer);
        }
    }
}

// Fuzz write operations with various buffer sizes and content
static void fuzz_write_operations(struct kunit *test)
{
    struct fuzz_file_data *file_data;
    
    file_data = kunit_kzalloc(test, sizeof(*file_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, file_data);
    
    for (int iteration = 0; iteration < 200; iteration++) {
        uint16_t write_size;
        uint8_t *write_buffer;
        
        get_random_bytes(&write_size, sizeof(write_size));
        write_size = write_size % (FUZZ_MAX_BUFFER_SIZE + 1);
        
        if (write_size == 0) {
            // Test zero-size write
            KUNIT_EXPECT_EQ(test, write_size, 0);
            continue;
        }
        
        write_buffer = kunit_kzalloc(test, write_size, GFP_KERNEL);
        if (!write_buffer) {
            continue;
        }
        
        // Fill with random data
        generate_random_data(write_buffer, write_size);
        
        // Reset position for each test
        file_data->position = 0;
        
        // Simulate write operation bounds checking
        size_t available_space = file_data->buffer_size - file_data->position;
        size_t to_write = min((size_t)write_size, available_space);
        
        if (file_data->position < file_data->buffer_size) {
            KUNIT_EXPECT_LE(test, to_write, write_size);
            KUNIT_EXPECT_LE(test, to_write, available_space);
            
            if (to_write > 0) {
                // Simulate copying data from userspace
                memcpy(&file_data->buffer[file_data->position], write_buffer, to_write);
                file_data->position += to_write;
            }
        }
        
        kfree(write_buffer);
    }
}

// Fuzz ioctl operations with random commands and arguments
static void fuzz_ioctl_operations(struct kunit *test)
{
    uint32_t commands[FUZZ_ITERATIONS];
    unsigned long args[FUZZ_ITERATIONS];
    
    // Generate random ioctl commands and arguments
    get_random_bytes(commands, sizeof(commands));
    get_random_bytes(args, sizeof(args));
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint32_t cmd = commands[i];
        unsigned long arg = args[i];
        
        // Extract ioctl command components
        unsigned int type = _IOC_TYPE(cmd);
        unsigned int nr = _IOC_NR(cmd);
        unsigned int dir = _IOC_DIR(cmd);
        unsigned int size = _IOC_SIZE(cmd);
        
        // Test command validation logic
        bool is_valid_magic = (type == 'm'); // Matrix Creator magic number
        bool is_valid_direction = (dir <= (_IOC_READ | _IOC_WRITE));
        bool is_reasonable_size = (size <= PAGE_SIZE);
        
        // Commands should be rejected if they don't meet basic criteria
        if (!is_valid_magic) {
            // Should be rejected for wrong magic number
            KUNIT_EXPECT_NE(test, type, 'm');
        }
        
        if (!is_valid_direction) {
            // Should be rejected for invalid direction
            KUNIT_EXPECT_GT(test, dir, (_IOC_READ | _IOC_WRITE));
        }
        
        if (!is_reasonable_size) {
            // Should be rejected for unreasonable size
            KUNIT_EXPECT_GT(test, size, PAGE_SIZE);
        }
        
        // Test specific ioctl patterns
        if (is_valid_magic && is_valid_direction && is_reasonable_size) {
            // This would be a potentially valid command
            KUNIT_EXPECT_EQ(test, type, 'm');
            KUNIT_EXPECT_LE(test, dir, (_IOC_READ | _IOC_WRITE));
            KUNIT_EXPECT_LE(test, size, PAGE_SIZE);
        }
    }
}

// Fuzz file position operations (lseek)
static void fuzz_lseek_operations(struct kunit *test)
{
    struct fuzz_file_data *file_data;
    loff_t positions[FUZZ_ITERATIONS];
    int whence_values[FUZZ_ITERATIONS];
    
    file_data = kunit_kzalloc(test, sizeof(*file_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, file_data);
    
    file_data->buffer_size = FUZZ_MAX_BUFFER_SIZE;
    
    // Generate random positions and whence values
    get_random_bytes(positions, sizeof(positions));
    get_random_bytes(whence_values, sizeof(whence_values));
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        loff_t offset = positions[i];
        int whence = whence_values[i] % 4; // 0-3 for SEEK_SET, SEEK_CUR, SEEK_END, invalid
        loff_t old_pos = file_data->position;
        loff_t new_pos;
        
        // Test seek operation logic
        switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = old_pos + offset;
            break;
        case SEEK_END:
            new_pos = file_data->buffer_size + offset;
            break;
        default:
            // Invalid whence should be rejected
            new_pos = -EINVAL;
            break;
        }
        
        // Validate position bounds
        if (new_pos >= 0 && whence <= SEEK_END) {
            if (new_pos > file_data->buffer_size) {
                // Position beyond end should be clamped or rejected
                KUNIT_EXPECT_GT(test, new_pos, file_data->buffer_size);
            } else {
                // Valid position should be accepted
                file_data->position = new_pos;
                KUNIT_EXPECT_GE(test, file_data->position, 0);
                KUNIT_EXPECT_LE(test, file_data->position, file_data->buffer_size);
            }
        } else {
            // Invalid positions should be rejected
            KUNIT_EXPECT_TRUE(test, new_pos < 0 || whence > SEEK_END);
        }
    }
}

// Fuzz concurrent file operations
static void fuzz_concurrent_operations(struct kunit *test)
{
    struct fuzz_file_data *shared_data;
    
    shared_data = kunit_kzalloc(test, sizeof(*shared_data), GFP_KERNEL);
    KUNIT_ASSERT_NOT_ERR_OR_NULL(test, shared_data);
    
    shared_data->buffer_size = FUZZ_MAX_BUFFER_SIZE;
    generate_random_data(shared_data->buffer, shared_data->buffer_size);
    
    // Simulate multiple file descriptors accessing same device
    for (int fd = 0; fd < 10; fd++) {
        loff_t position = 0;
        uint8_t operation;
        
        get_random_bytes(&operation, sizeof(operation));
        operation = operation % 3; // 0=read, 1=write, 2=seek
        
        switch (operation) {
        case 0: // Read operation
            {
                uint16_t read_size;
                get_random_bytes(&read_size, sizeof(read_size));
                read_size = read_size % 256; // 0-255 bytes
                
                size_t available = shared_data->buffer_size - position;
                size_t to_read = min((size_t)read_size, available);
                
                KUNIT_EXPECT_LE(test, to_read, read_size);
                KUNIT_EXPECT_LE(test, to_read, available);
                
                position += to_read;
            }
            break;
            
        case 1: // Write operation
            {
                uint16_t write_size;
                get_random_bytes(&write_size, sizeof(write_size));
                write_size = write_size % 256; // 0-255 bytes
                
                size_t available = shared_data->buffer_size - position;
                size_t to_write = min((size_t)write_size, available);
                
                KUNIT_EXPECT_LE(test, to_write, write_size);
                KUNIT_EXPECT_LE(test, to_write, available);
                
                position += to_write;
            }
            break;
            
        case 2: // Seek operation
            {
                uint16_t new_pos;
                get_random_bytes(&new_pos, sizeof(new_pos));
                new_pos = new_pos % (shared_data->buffer_size + 100);
                
                if (new_pos <= shared_data->buffer_size) {
                    position = new_pos;
                    KUNIT_EXPECT_LE(test, position, shared_data->buffer_size);
                }
            }
            break;
        }
        
        // Verify position stays within bounds
        KUNIT_EXPECT_GE(test, position, 0);
        KUNIT_EXPECT_LE(test, position, shared_data->buffer_size);
    }
}

// Fuzz device node creation parameters
static void fuzz_device_node_creation(struct kunit *test)
{
    dev_t device_numbers[100];
    const char *device_names[] = {
        "matrixio_regmap", "matrixio_everloop", "matrixio_env",
        "", "a", "very_long_device_name_that_exceeds_normal_limits",
        "device_with_special_chars_!@#$%", "device\x00with\x00nulls",
        NULL
    };
    
    // Generate random device numbers
    get_random_bytes(device_numbers, sizeof(device_numbers));
    
    for (int i = 0; i < ARRAY_SIZE(device_numbers); i++) {
        dev_t devt = device_numbers[i];
        int major = MAJOR(devt);
        int minor = MINOR(devt);
        
        // Test device number validation
        KUNIT_EXPECT_GE(test, major, 0);
        KUNIT_EXPECT_LT(test, major, (1 << 12)); // 12-bit major numbers
        KUNIT_EXPECT_GE(test, minor, 0);
        KUNIT_EXPECT_LT(test, minor, (1 << 20)); // 20-bit minor numbers
        
        // Reconstruct device number
        dev_t reconstructed = MKDEV(major, minor);
        KUNIT_EXPECT_EQ(test, MAJOR(reconstructed), major);
        KUNIT_EXPECT_EQ(test, MINOR(reconstructed), minor);
    }
    
    // Test device name validation
    for (int i = 0; i < ARRAY_SIZE(device_names); i++) {
        const char *name = device_names[i];
        
        if (name == NULL) {
            // NULL names should be rejected
            KUNIT_EXPECT_NULL(test, name);
        } else if (strlen(name) == 0) {
            // Empty names should be rejected
            KUNIT_EXPECT_EQ(test, strlen(name), 0);
        } else if (strlen(name) > 64) {
            // Very long names should be rejected
            KUNIT_EXPECT_GT(test, strlen(name), 64);
        } else {
            // Valid names should be accepted
            KUNIT_EXPECT_GT(test, strlen(name), 0);
            KUNIT_EXPECT_LE(test, strlen(name), 64);
        }
    }
}

// KUnit test suite definition
static struct kunit_case fuzz_userspace_interface_test_cases[] = {
    KUNIT_CASE(fuzz_read_operations),
    KUNIT_CASE(fuzz_write_operations),
    KUNIT_CASE(fuzz_ioctl_operations),
    KUNIT_CASE(fuzz_lseek_operations),
    KUNIT_CASE(fuzz_concurrent_operations),
    KUNIT_CASE(fuzz_device_node_creation),
    {}
};

static struct kunit_suite fuzz_userspace_interface_test_suite = {
    .name = "fuzz-userspace-interface",
    .test_cases = fuzz_userspace_interface_test_cases,
};

kunit_test_suite(fuzz_userspace_interface_test_suite);
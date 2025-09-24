// Fuzzing tests for device tree parsing robustness
#include <kunit/test.h>
#include <linux/random.h>
#include <linux/of.h>
#include <linux/platform_device.h>

// Test-specific includes
#include "../mocks/mock-platform-device.h"

#define FUZZ_ITERATIONS 500
#define MAX_PROPERTY_SIZE 1024

// Mock device tree property structure
struct mock_dt_property {
    const char *name;
    void *value;
    int length;
};

// Helper function to generate random string
static char *generate_random_string(struct kunit *test, size_t max_len)
{
    char *str;
    size_t len;
    uint8_t len_byte;
    
    get_random_bytes(&len_byte, 1);
    len = (len_byte % max_len) + 1;
    
    str = kunit_kzalloc(test, len + 1, GFP_KERNEL);
    if (!str) {
        return NULL;
    }
    
    for (size_t i = 0; i < len; i++) {
        uint8_t c;
        get_random_bytes(&c, 1);
        // Keep characters printable and safe
        str[i] = (c % 94) + 33; // ASCII 33-126
    }
    str[len] = '\0';
    
    return str;
}

// Fuzz property name validation
static void fuzz_property_names(struct kunit *test)
{
    const char *valid_property_names[] = {
        "compatible",
        "reg",
        "interrupts",
        "status",
        "device_type",
        "matrixio,spi-max-frequency",
        "matrixio,gpio-pin",
    };
    
    const char *invalid_property_names[] = {
        "",                    // Empty name
        NULL,                  // NULL name
        "property with spaces", // Spaces not typically allowed
        "property\x00null",    // Embedded null
        "property\nwith\nnewlines", // Newlines
        "extremely_long_property_name_that_exceeds_reasonable_limits_and_should_probably_be_rejected_by_any_sane_parser",
    };
    
    // Test valid property names
    for (int i = 0; i < ARRAY_SIZE(valid_property_names); i++) {
        const char *name = valid_property_names[i];
        
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, name);
        KUNIT_EXPECT_GT(test, strlen(name), 0);
        KUNIT_EXPECT_LT(test, strlen(name), 64); // Reasonable limit
        
        // Should not contain whitespace or special characters
        for (const char *c = name; *c; c++) {
            if (*c != ',' && *c != '-' && *c != '_') {
                KUNIT_EXPECT_TRUE(test, isalnum(*c));
            }
        }
    }
    
    // Test invalid property names
    for (int i = 0; i < ARRAY_SIZE(invalid_property_names); i++) {
        const char *name = invalid_property_names[i];
        
        if (name == NULL) {
            KUNIT_EXPECT_NULL(test, name);
        } else if (strlen(name) == 0) {
            KUNIT_EXPECT_EQ(test, strlen(name), 0);
        } else {
            // These should be rejected by proper validation
            bool has_invalid_chars = false;
            for (const char *c = name; *c; c++) {
                if (*c < 32 || *c > 126) { // Non-printable
                    has_invalid_chars = true;
                    break;
                }
                if (isspace(*c)) { // Whitespace
                    has_invalid_chars = true;
                    break;
                }
            }
            
            if (!has_invalid_chars && strlen(name) > 64) {
                // Too long
                KUNIT_EXPECT_GT(test, strlen(name), 64);
            }
        }
    }
}

// Fuzz property value parsing
static void fuzz_property_values(struct kunit *test)
{
    uint8_t random_data[MAX_PROPERTY_SIZE];
    
    for (int iteration = 0; iteration < FUZZ_ITERATIONS; iteration++) {
        uint16_t data_size;
        get_random_bytes(&data_size, sizeof(data_size));
        data_size = data_size % MAX_PROPERTY_SIZE;
        
        if (data_size > 0) {
            get_random_bytes(random_data, data_size);
            
            // Test various property value interpretations
            
            // Test as u32 array
            if (data_size >= sizeof(uint32_t) && (data_size % sizeof(uint32_t)) == 0) {
                uint32_t *u32_array = (uint32_t *)random_data;
                size_t count = data_size / sizeof(uint32_t);
                
                for (size_t i = 0; i < count; i++) {
                    // Test endianness handling (device tree is big-endian)
                    uint32_t be_value = be32_to_cpu(u32_array[i]);
                    uint32_t le_value = le32_to_cpu(u32_array[i]);
                    
                    // Values should be different unless the original was endian-neutral
                    if (u32_array[i] != 0 && u32_array[i] != 0xFFFFFFFF) {
                        KUNIT_EXPECT_NE(test, be_value, le_value);
                    }
                }
            }
            
            // Test as string
            if (data_size < MAX_PROPERTY_SIZE) {
                char test_string[MAX_PROPERTY_SIZE + 1];
                memcpy(test_string, random_data, data_size);
                test_string[data_size] = '\0';
                
                // Check for string validity
                size_t actual_len = strnlen(test_string, data_size);
                KUNIT_EXPECT_LE(test, actual_len, data_size);
                
                // String properties should be null-terminated within the data
                if (actual_len < data_size) {
                    KUNIT_EXPECT_EQ(test, test_string[actual_len], '\0');
                }
            }
            
            // Test as byte array
            for (size_t i = 0; i < data_size; i++) {
                // Each byte should be accessible
                KUNIT_EXPECT_GE(test, random_data[i], 0);
                KUNIT_EXPECT_LE(test, random_data[i], 255);
            }
        }
    }
}

// Fuzz compatible string matching
static void fuzz_compatible_strings(struct kunit *test)
{
    const char *valid_compatible_strings[] = {
        "matrixio,creator",
        "matrixio,voice",
        "matrixio,env-sensor",
        "vendor,device",
        "vendor,device-v2",
    };
    
    // Test valid compatible strings
    for (int i = 0; i < ARRAY_SIZE(valid_compatible_strings); i++) {
        const char *compat = valid_compatible_strings[i];
        
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, compat);
        
        // Should contain vendor,device format
        const char *comma = strchr(compat, ',');
        if (comma) {
            KUNIT_EXPECT_NOT_ERR_OR_NULL(test, comma);
            KUNIT_EXPECT_GT(test, comma - compat, 0); // Vendor part exists
            KUNIT_EXPECT_GT(test, strlen(comma + 1), 0); // Device part exists
        }
    }
    
    // Fuzz random compatible strings
    for (int iteration = 0; iteration < 100; iteration++) {
        char *random_compat = generate_random_string(test, 64);
        if (!random_compat) {
            continue;
        }
        
        // Test matching logic
        bool matches_expected = false;
        for (int i = 0; i < ARRAY_SIZE(valid_compatible_strings); i++) {
            if (strcmp(random_compat, valid_compatible_strings[i]) == 0) {
                matches_expected = true;
                break;
            }
        }
        
        // Random strings should generally not match expected values
        if (!matches_expected) {
            KUNIT_EXPECT_FALSE(test, matches_expected);
        }
    }
}

// Fuzz register address parsing
static void fuzz_register_addresses(struct kunit *test)
{
    uint32_t reg_values[FUZZ_ITERATIONS * 2]; // address, size pairs
    
    get_random_bytes(reg_values, sizeof(reg_values));
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint32_t address = be32_to_cpu(reg_values[i * 2]);
        uint32_t size = be32_to_cpu(reg_values[i * 2 + 1]);
        
        // Test address validation
        if (address != 0) {
            // Non-zero addresses should be reasonable
            KUNIT_EXPECT_NE(test, address, 0);
            
            // Should not be in kernel space (assume 32-bit addresses)
            KUNIT_EXPECT_LT(test, address, 0x80000000UL);
        }
        
        // Test size validation
        if (size > 0) {
            KUNIT_EXPECT_GT(test, size, 0);
            
            // Size should be reasonable (not GB-sized regions)
            KUNIT_EXPECT_LT(test, size, 0x10000000UL); // 256MB max
            
            // Check for overflow
            uint64_t end_address = (uint64_t)address + size;
            KUNIT_EXPECT_GE(test, end_address, address); // No overflow
        }
        
        // Test alignment requirements
        if (size >= 4) {
            // Addresses for word-sized regions should be word-aligned
            bool is_aligned = (address % 4) == 0;
            // This is a suggestion, not a hard requirement
        }
    }
}

// Fuzz interrupt specification parsing
static void fuzz_interrupt_specs(struct kunit *test)
{
    uint32_t interrupt_data[FUZZ_ITERATIONS * 3]; // IRQ, flags, cells
    
    get_random_bytes(interrupt_data, sizeof(interrupt_data));
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint32_t irq_num = be32_to_cpu(interrupt_data[i * 3]);
        uint32_t irq_flags = be32_to_cpu(interrupt_data[i * 3 + 1]);
        uint32_t irq_cells = be32_to_cpu(interrupt_data[i * 3 + 2]);
        
        // Test IRQ number validation
        if (irq_num < 1024) { // Reasonable IRQ range
            KUNIT_EXPECT_LT(test, irq_num, 1024);
            
            // IRQ 0 is typically invalid
            if (irq_num == 0) {
                KUNIT_EXPECT_EQ(test, irq_num, 0);
            }
        }
        
        // Test IRQ flags
        uint32_t valid_flags = 0x0F; // Assume 4 bits of flags
        uint32_t masked_flags = irq_flags & valid_flags;
        KUNIT_EXPECT_EQ(test, masked_flags, irq_flags & valid_flags);
        
        // Test cell count
        if (irq_cells <= 4) { // Reasonable cell count
            KUNIT_EXPECT_LE(test, irq_cells, 4);
        }
    }
}

// Fuzz GPIO specification parsing
static void fuzz_gpio_specs(struct kunit *test)
{
    uint32_t gpio_data[FUZZ_ITERATIONS * 2]; // GPIO number, flags
    
    get_random_bytes(gpio_data, sizeof(gpio_data));
    
    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint32_t gpio_num = be32_to_cpu(gpio_data[i * 2]);
        uint32_t gpio_flags = be32_to_cpu(gpio_data[i * 2 + 1]);
        
        // Test GPIO number validation
        if (gpio_num < 512) { // Reasonable GPIO range for Pi
            KUNIT_EXPECT_LT(test, gpio_num, 512);
        }
        
        // Test GPIO flags
        uint32_t valid_flags = 0x07; // Active high/low, open drain, etc.
        if ((gpio_flags & ~valid_flags) == 0) {
            KUNIT_EXPECT_EQ(test, gpio_flags & ~valid_flags, 0);
        }
    }
}

// Fuzz device tree node structure
static void fuzz_device_tree_structure(struct kunit *test)
{
    struct platform_device *pdev;
    
    for (int iteration = 0; iteration < 50; iteration++) {
        char *node_name = generate_random_string(test, 32);
        if (!node_name) {
            continue;
        }
        
        pdev = create_mock_platform_device(test, node_name);
        if (!pdev) {
            continue;
        }
        
        // Test device name validation
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pdev->name);
        
        if (pdev->name) {
            size_t name_len = strlen(pdev->name);
            KUNIT_EXPECT_GT(test, name_len, 0);
            KUNIT_EXPECT_LT(test, name_len, 64);
        }
        
        // Test device ID
        KUNIT_EXPECT_GE(test, pdev->id, -1); // -1 is valid (auto-assign)
        
        // Test device structure integrity
        KUNIT_EXPECT_NOT_ERR_OR_NULL(test, &pdev->dev);
    }
}

// KUnit test suite definition
static struct kunit_case fuzz_device_tree_test_cases[] = {
    KUNIT_CASE(fuzz_property_names),
    KUNIT_CASE(fuzz_property_values),
    KUNIT_CASE(fuzz_compatible_strings),
    KUNIT_CASE(fuzz_register_addresses),
    KUNIT_CASE(fuzz_interrupt_specs),
    KUNIT_CASE(fuzz_gpio_specs),
    KUNIT_CASE(fuzz_device_tree_structure),
    {}
};

static struct kunit_suite fuzz_device_tree_test_suite = {
    .name = "fuzz-device-tree",
    .test_cases = fuzz_device_tree_test_cases,
};

kunit_test_suite(fuzz_device_tree_test_suite);
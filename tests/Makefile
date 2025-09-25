CONFIG_MATRIXIO_KUNIT_TEST ?= y

# KUnit test configuration
obj-$(CONFIG_MATRIXIO_KUNIT_TEST) += matrixio-test-suite.o

matrixio-test-suite-y := \
	unit/test-matrixio-core.o \
	unit/test-matrixio-regmap.o \
	unit/test-matrixio-env.o \
	unit/test-matrixio-everloop.o \
	fuzz/fuzz-spi-interface.o \
	fuzz/fuzz-userspace-interface.o \
	fuzz/fuzz-device-tree.o \
	integration/test-full-stack.o \
	mocks/mock-spi.o \
	mocks/mock-iio.o \
	mocks/mock-platform-device.o

# Include source directory for headers
ccflags-y += -I$(src)/../src
ccflags-y += -I$(src)/mocks
ccflags-y += -DCONFIG_MATRIXIO_TESTING=1

# Enable KUnit framework
ccflags-y += -DKUNIT_TESTING

# Test-specific compiler flags
ccflags-y += -Wno-missing-prototypes
ccflags-y += -Wno-missing-declarations

.PHONY: test clean-test

test:
	@echo "Running Matrix Creator KUnit tests..."
	@if command -v kunit.py >/dev/null 2>&1; then \
		kunit.py run --kunitconfig=.kunitconfig; \
	else \
		echo "KUnit not available, running basic compilation test"; \
		$(MAKE) -C $(KDIR) M=$(PWD) modules; \
	fi

clean-test:
	@echo "Cleaning test artifacts..."
	@rm -f *.o *.ko *.mod *.mod.c .*.cmd *.order *.symvers
	@find . -name "*.o" -delete
	@find . -name ".*.cmd" -delete
	@find . -name "*.mod.c" -delete
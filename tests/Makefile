# Matrix Creator Kernel Module Testing Makefile
# Usage: make [target]

.PHONY: all build test baseline new clean help install-deps check-deps

# Configuration
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra
LDFLAGS = -lmatrix_creator_hal
TEST_BINARY = matrix_test_working
TEST_SOURCE = matrix_test_working.cpp
REGRESSION_SCRIPT = matrix_regression_test.sh
RESULTS_DIR = /tmp/matrix_test_results

# Default target
all: help

# Build the test binary
build: check-deps $(TEST_BINARY)

$(TEST_BINARY): $(TEST_SOURCE)
	@echo "🔨 Building Matrix compatibility test..."
	$(CXX) $(CXXFLAGS) -o $(TEST_BINARY) $(TEST_SOURCE) $(LDFLAGS)
	@echo "✅ Test binary compiled successfully"

# Check dependencies
check-deps:
	@echo "🔍 Checking dependencies..."
	@command -v $(CXX) >/dev/null 2>&1 || { echo "❌ g++ not found. Install with: sudo apt install build-essential"; exit 1; }
	@ls /usr/local/lib/libmatrix_creator_hal.so >/dev/null 2>&1 || { echo "❌ Matrix HAL library not found. Please install libmatrix_creator_hal"; exit 1; }
	@test -c /dev/spidev0.0 || test -c /dev/spidev0.1 || { echo "❌ SPI device not found. Enable SPI in raspi-config"; exit 1; }
	@echo "✅ All dependencies satisfied"

# Install build dependencies
install-deps:
	@echo "📦 Installing build dependencies..."
	sudo apt update
	sudo apt install -y build-essential raspberrypi-kernel-headers
	@echo "✅ Dependencies installed"

# Run direct test
test: build
	@echo "🧪 Running Matrix compatibility test..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "⚠️  Test requires root privileges. Running with sudo..."; \
		sudo ./$(TEST_BINARY); \
	else \
		./$(TEST_BINARY); \
	fi

# Run baseline regression test
baseline: build
	@echo "📊 Running baseline regression test..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "⚠️  Test requires root privileges. Running with sudo..."; \
		sudo ./$(REGRESSION_SCRIPT) baseline "Baseline test $$(date +%Y%m%d_%H%M%S)"; \
	else \
		./$(REGRESSION_SCRIPT) baseline "Baseline test $$(date +%Y%m%d_%H%M%S)"; \
	fi

# Run new modules regression test
new: build
	@echo "🆕 Running new modules regression test..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "⚠️  Test requires root privileges. Running with sudo..."; \
		sudo ./$(REGRESSION_SCRIPT) new "New modules test $$(date +%Y%m%d_%H%M%S)"; \
	else \
		./$(REGRESSION_SCRIPT) new "New modules test $$(date +%Y%m%d_%H%M%S)"; \
	fi

# Custom regression test with description
regression: build
	@if [ -z "$(DESC)" ]; then \
		echo "❌ Please provide a description: make regression DESC='Test description'"; \
		exit 1; \
	fi
	@echo "🔄 Running regression test: $(DESC)"
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "⚠️  Test requires root privileges. Running with sudo..."; \
		sudo ./$(REGRESSION_SCRIPT) $(TYPE) "$(DESC)"; \
	else \
		./$(REGRESSION_SCRIPT) $(TYPE) "$(DESC)"; \
	fi

# Show test results
results:
	@echo "📋 Recent test results:"
	@if [ -d "$(RESULTS_DIR)" ]; then \
		ls -la $(RESULTS_DIR)/ | head -10; \
		echo ""; \
		echo "Latest test summary:"; \
		ls -t $(RESULTS_DIR)/*summary.txt 2>/dev/null | head -1 | xargs cat 2>/dev/null || echo "No summary files found"; \
	else \
		echo "No test results found in $(RESULTS_DIR)"; \
	fi

# Check module status
status:
	@echo "🔍 Matrix kernel module status:"
	@echo "Loaded modules:"
	@lsmod | grep matrix || echo "No Matrix modules loaded"
	@echo ""
	@echo "Module files:"
	@find /lib/modules/$$(uname -r) -name "*matrix*" -type f 2>/dev/null | head -10 || echo "No Matrix module files found"
	@echo ""
	@echo "SPI device:"
	@ls -la /dev/spi* 2>/dev/null || echo "No SPI devices found"

# Reload Matrix modules
reload-modules:
	@echo "🔄 Reloading Matrix kernel modules..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "⚠️  Module operations require root privileges. Running with sudo..."; \
		sudo modprobe -r matrixio_everloop matrixio_codec matrixio_playback matrixio_gpio matrixio_imu matrixio_mic matrixio_env matrixio_uart matrixio_regmap matrixio_core 2>/dev/null || true; \
		sudo modprobe matrixio_core; \
		sudo modprobe matrixio_regmap; \
		sudo modprobe matrixio_everloop; \
		sudo modprobe matrixio_gpio; \
		sudo modprobe matrixio_imu; \
		sudo modprobe matrixio_env; \
		sudo modprobe matrixio_uart; \
	else \
		modprobe -r matrixio_everloop matrixio_codec matrixio_playback matrixio_gpio matrixio_imu matrixio_mic matrixio_env matrixio_uart matrixio_regmap matrixio_core 2>/dev/null || true; \
		modprobe matrixio_core; \
		modprobe matrixio_regmap; \
		modprobe matrixio_everloop; \
		modprobe matrixio_gpio; \
		modprobe matrixio_imu; \
		modprobe matrixio_env; \
		modprobe matrixio_uart; \
	fi
	@echo "✅ Modules reloaded"

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f $(TEST_BINARY) *.o
	@echo "✅ Clean complete"

# Full clean including test results
clean-all: clean
	@echo "🧹 Cleaning all test artifacts..."
	rm -rf $(RESULTS_DIR)
	@echo "✅ Full clean complete"

# Validation workflow
validate: check-deps build baseline
	@echo ""
	@echo "🎯 Validation workflow completed"
	@echo "Now install your new modules and run: make new"

# Help target
help:
	@echo "Matrix Creator Kernel Module Testing"
	@echo "===================================="
	@echo ""
	@echo "Available targets:"
	@echo "  build              - Compile the test binary"
	@echo "  test               - Run compatibility test"
	@echo "  baseline           - Run baseline regression test"
	@echo "  new                - Run new modules regression test"
	@echo "  regression         - Custom regression test (requires DESC=\"description\")"
	@echo "  results            - Show recent test results"
	@echo "  status             - Check Matrix module status"
	@echo "  reload-modules     - Reload Matrix kernel modules"
	@echo "  validate           - Full validation workflow (deps + build + baseline)"
	@echo "  install-deps       - Install build dependencies"
	@echo "  check-deps         - Check if dependencies are satisfied"
	@echo "  clean              - Remove build artifacts"
	@echo "  clean-all          - Remove all artifacts including test results"
	@echo "  help               - Show this help"
	@echo ""
	@echo "Examples:"
	@echo "  make validate                                    # Full validation setup"
	@echo "  make baseline                                    # Establish baseline"
	@echo "  make new                                         # Test new modules"
	@echo "  make regression TYPE=new DESC=\"My new modules\"   # Custom test"
	@echo "  make test                                        # Quick compatibility test"
	@echo "  make status                                      # Check module status"
	@echo ""
	@echo "Workflow:"
	@echo "  1. make validate      (setup and baseline)"
	@echo "  2. Install new modules"
	@echo "  3. make new           (test new modules)"
	@echo "  4. make results       (view comparison)"

# Debug target for development
debug: CXXFLAGS += -g -DDEBUG
debug: build
	@echo "🐛 Debug build complete"
#!/bin/bash

# Matrix Creator Kernel Module Regression Test Script
# Usage: ./matrix_regression_test.sh [baseline|new] [test_description]

set -e

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
TEST_BINARY="$SCRIPT_DIR/matrix_test_working"
RESULTS_DIR="/tmp/matrix_test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
    echo "Matrix Creator Regression Test Script"
    echo "Usage: $0 [baseline|new] [description]"
    echo ""
    echo "baseline - Run test with current/baseline modules"
    echo "new      - Run test with new modules for comparison"
    echo ""
    echo "Examples:"
    echo "  $0 baseline 'Original modules v0.2.5'"
    echo "  $0 new 'kai5263499 modules v0.3.1'"
    echo ""
    echo "Results are saved to $RESULTS_DIR/"
    exit 1
}

check_prerequisites() {
    echo -e "${BLUE}[INFO]${NC} Checking prerequisites..."
    
    # Check if test binary exists
    if [[ ! -f "$TEST_BINARY" ]]; then
        echo -e "${RED}[ERROR]${NC} Test binary not found: $TEST_BINARY"
        echo "Please compile the test first: g++ -o matrix_test_working matrix_test_working.cpp -lmatrix_creator_hal -std=c++11"
        exit 1
    fi
    
    # Check if running as root (needed for hardware access)
    if [[ $EUID -ne 0 ]]; then
        echo -e "${RED}[ERROR]${NC} This script must be run as root (sudo)"
        exit 1
    fi
    
    # Create results directory
    mkdir -p "$RESULTS_DIR"
    
    echo -e "${GREEN}[OK]${NC} Prerequisites check passed"
}

check_kernel_modules() {
    echo -e "${BLUE}[INFO]${NC} Checking Matrix kernel modules..."
    
    # Check loaded modules
    LOADED_MODULES=$(lsmod | grep matrix | wc -l)
    if [[ $LOADED_MODULES -eq 0 ]]; then
        echo -e "${YELLOW}[WARNING]${NC} No Matrix modules loaded"
        echo "Attempting to load modules..."
        
        # Try to load core modules
        modprobe matrixio_core 2>/dev/null || echo "Failed to load matrixio_core"
        modprobe matrixio_regmap 2>/dev/null || echo "Failed to load matrixio_regmap"
        modprobe matrixio_everloop 2>/dev/null || echo "Failed to load matrixio_everloop"
        modprobe matrixio_gpio 2>/dev/null || echo "Failed to load matrixio_gpio"
        modprobe matrixio_imu 2>/dev/null || echo "Failed to load matrixio_imu"
        modprobe matrixio_env 2>/dev/null || echo "Failed to load matrixio_env"
        modprobe matrixio_uart 2>/dev/null || echo "Failed to load matrixio_uart"
        
        LOADED_MODULES=$(lsmod | grep matrix | wc -l)
    fi
    
    echo "Loaded Matrix modules: $LOADED_MODULES"
    lsmod | grep matrix
    
    # Check module versions and sources
    echo -e "\n${BLUE}[INFO]${NC} Module information:"
    for module in matrixio_core matrixio_regmap matrixio_everloop matrixio_gpio matrixio_imu matrixio_env matrixio_uart; do
        if lsmod | grep -q $module; then
            MODULE_PATH=$(find /lib/modules/$(uname -r) -name "${module}.ko" 2>/dev/null | head -1)
            if [[ -n "$MODULE_PATH" ]]; then
                MODULE_DATE=$(stat -c %y "$MODULE_PATH" | cut -d' ' -f1)
                echo "  $module: $MODULE_PATH (modified: $MODULE_DATE)"
            fi
        fi
    done
}

run_test() {
    local test_type="$1"
    local description="$2"
    local output_file="$RESULTS_DIR/test_${test_type}_${TIMESTAMP}.log"
    local summary_file="$RESULTS_DIR/test_${test_type}_${TIMESTAMP}_summary.txt"
    
    echo -e "${BLUE}[INFO]${NC} Running $test_type test: $description"
    echo "Output will be saved to: $output_file"
    
    # Create test metadata
    cat > "$summary_file" << EOL
Matrix Creator Kernel Module Test Results
==========================================
Test Type: $test_type
Description: $description
Timestamp: $(date)
Kernel Version: $(uname -r)
Matrix HAL Version: $(pkg-config --modversion matrix-creator-hal 2>/dev/null || echo "Unknown")

Loaded Modules:
$(lsmod | grep matrix)

Hardware Information:
$(cat /proc/cpuinfo | grep "Model" | head -1)
$(cat /proc/device-tree/model 2>/dev/null || echo "Model: Unknown")

EOL
    
    # Run the test and capture output
    echo -e "${YELLOW}[RUNNING]${NC} Executing test..."
    
    if cd "$SCRIPT_DIR" && timeout 60s ./$(basename "$TEST_BINARY") > "$output_file" 2>&1; then
        # Parse test results
        TOTAL_TESTS=$(grep "Total Tests:" "$output_file" | awk '{print $3}')
        PASSED_TESTS=$(grep "Passed:" "$output_file" | awk '{print $2}')
        FAILED_TESTS=$(grep "Failed:" "$output_file" | awk '{print $2}')
        SUCCESS_RATE=$(grep "Success Rate:" "$output_file" | awk '{print $3}')
        
        # Add results to summary
        cat >> "$summary_file" << EOL

Test Results:
=============
Total Tests: $TOTAL_TESTS
Passed: $PASSED_TESTS
Failed: $FAILED_TESTS
Success Rate: $SUCCESS_RATE

EOL
        
        # Show result
        if [[ "$FAILED_TESTS" == "0" ]]; then
            echo -e "${GREEN}[SUCCESS]${NC} All tests passed! ($SUCCESS_RATE success rate)"
        else
            echo -e "${RED}[PARTIAL]${NC} $FAILED_TESTS tests failed ($SUCCESS_RATE success rate)"
        fi
        
        # Show any failed tests
        if [[ "$FAILED_TESTS" != "0" ]]; then
            echo -e "${YELLOW}[INFO]${NC} Failed tests:"
            grep "FAIL" "$output_file" | head -10
        fi
        
    else
        echo -e "${RED}[ERROR]${NC} Test execution failed"
        cat "$output_file"
        return 1
    fi
    
    echo "Test completed. Full output: $output_file"
    echo "Summary: $summary_file"
}

compare_results() {
    echo -e "${BLUE}[INFO]${NC} Comparing test results..."
    
    # Find the latest baseline and new test results
    BASELINE_LOG=$(ls -t "$RESULTS_DIR"/test_baseline_*.log 2>/dev/null | head -1)
    NEW_LOG=$(ls -t "$RESULTS_DIR"/test_new_*.log 2>/dev/null | head -1)
    
    if [[ -z "$BASELINE_LOG" ]]; then
        echo -e "${YELLOW}[WARNING]${NC} No baseline results found"
        return 0
    fi
    
    if [[ -z "$NEW_LOG" ]]; then
        echo -e "${YELLOW}[WARNING]${NC} No new test results found"
        return 0
    fi
    
    echo "Comparing:"
    echo "  Baseline: $BASELINE_LOG"
    echo "  New:      $NEW_LOG"
    
    # Extract key metrics
    BASELINE_SUCCESS=$(grep "Success Rate:" "$BASELINE_LOG" | awk '{print $3}' | sed 's/%//')
    NEW_SUCCESS=$(grep "Success Rate:" "$NEW_LOG" | awk '{print $3}' | sed 's/%//')
    
    BASELINE_FAILED=$(grep "Failed:" "$BASELINE_LOG" | awk '{print $2}')
    NEW_FAILED=$(grep "Failed:" "$NEW_LOG" | awk '{print $2}')
    
    echo ""
    echo "Comparison Results:"
    echo "  Baseline Success Rate: $BASELINE_SUCCESS%"
    echo "  New Success Rate:      $NEW_SUCCESS%"
    echo "  Baseline Failed Tests: $BASELINE_FAILED"
    echo "  New Failed Tests:      $NEW_FAILED"
    
    # Determine compatibility
    if [[ "$NEW_SUCCESS" == "$BASELINE_SUCCESS" ]] && [[ "$NEW_FAILED" == "$BASELINE_FAILED" ]]; then
        echo -e "${GREEN}[COMPATIBLE]${NC} New modules show identical test results to baseline"
    elif [[ "$NEW_FAILED" -lt "$BASELINE_FAILED" ]]; then
        echo -e "${GREEN}[IMPROVED]${NC} New modules show better results than baseline"
    else
        echo -e "${RED}[REGRESSION]${NC} New modules show worse results than baseline"
        
        # Show which tests regressed
        echo -e "${YELLOW}[INFO]${NC} Checking for specific regressions..."
        diff -u "$BASELINE_LOG" "$NEW_LOG" | grep -E "^[+-].*FAIL" || echo "No specific test failures identified"
    fi
}

generate_report() {
    local report_file="$RESULTS_DIR/compatibility_report_$TIMESTAMP.md"
    
    echo -e "${BLUE}[INFO]${NC} Generating compatibility report: $report_file"
    
    cat > "$report_file" << 'EOL'
# Matrix Creator Kernel Module Compatibility Report

## Test Summary

This report compares the functionality of different Matrix Creator kernel module versions to ensure backwards compatibility.

### System Information
EOL
    
    echo "- **Date**: $(date)" >> "$report_file"
    echo "- **Kernel**: $(uname -r)" >> "$report_file"
    echo "- **Hardware**: $(cat /proc/device-tree/model 2>/dev/null || echo "Unknown")" >> "$report_file"
    echo "" >> "$report_file"
    
    # Add test results if available
    if [[ -f "$BASELINE_LOG" ]] && [[ -f "$NEW_LOG" ]]; then
        echo "### Test Results Comparison" >> "$report_file"
        echo "" >> "$report_file"
        echo "| Metric | Baseline | New | Status |" >> "$report_file"
        echo "|--------|----------|-----|--------|" >> "$report_file"
        
        BASELINE_SUCCESS=$(grep "Success Rate:" "$BASELINE_LOG" | awk '{print $3}')
        NEW_SUCCESS=$(grep "Success Rate:" "$NEW_LOG" | awk '{print $3}')
        BASELINE_FAILED=$(grep "Failed:" "$BASELINE_LOG" | awk '{print $2}')
        NEW_FAILED=$(grep "Failed:" "$NEW_LOG" | awk '{print $2}')
        
        STATUS="✅ Compatible"
        if [[ "$NEW_FAILED" -gt "$BASELINE_FAILED" ]]; then
            STATUS="❌ Regression"
        elif [[ "$NEW_FAILED" -lt "$BASELINE_FAILED" ]]; then
            STATUS="🎉 Improved"
        fi
        
        echo "| Success Rate | $BASELINE_SUCCESS | $NEW_SUCCESS | $STATUS |" >> "$report_file"
        echo "| Failed Tests | $BASELINE_FAILED | $NEW_FAILED | $STATUS |" >> "$report_file"
    fi
    
    echo "Report generated: $report_file"
}

main() {
    if [[ $# -lt 1 ]]; then
        usage
    fi
    
    local test_type="$1"
    local description="${2:-No description provided}"
    
    if [[ "$test_type" != "baseline" ]] && [[ "$test_type" != "new" ]]; then
        echo -e "${RED}[ERROR]${NC} Invalid test type: $test_type"
        usage
    fi
    
    echo "Matrix Creator Kernel Module Regression Test"
    echo "============================================="
    echo "Test Type: $test_type"
    echo "Description: $description"
    echo ""
    
    check_prerequisites
    check_kernel_modules
    run_test "$test_type" "$description"
    
    if [[ "$test_type" == "new" ]]; then
        compare_results
        generate_report
    fi
    
    echo ""
    echo -e "${GREEN}[COMPLETE]${NC} Test run finished"
    echo "Results directory: $RESULTS_DIR"
}

main "$@"

DTC?=dtc
# Allow KDIR to be overridden, fallback to system kernel if not set
KDIR?=/lib/modules/$(shell uname -r)/build

all: matrixio.dtbo
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

matrixio.dtbo: matrixio.dts
	$(DTC) -W no-unit_address_vs_reg -@ -I dts -O dtb -o matrixio.dtbo matrixio.dts

install: matrixio.dtbo
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install
	depmod -A
	cp matrixio.dtbo /boot/overlays

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f matrixio.dtbo

# Clean all build test artifacts
clean-tests:
	@echo "Cleaning build test artifacts..."
	rm -rf ../build-test-*
	@echo "Build test directories cleaned"

# Clean everything including test artifacts  
clean-all: clean clean-tests
	@echo "All build artifacts cleaned"

# Debug target to show variables
debug:
	@echo "KDIR: $(KDIR)"
	@echo "ARCH: $(ARCH)"
	@echo "CROSS_COMPILE: $(CROSS_COMPILE)"
	@echo "CURDIR: $(CURDIR)"

.PHONY: all install clean clean-tests clean-all debug

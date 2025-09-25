# Matrix Creator Kernel Modules Makefile
# Development and CI/CD utilities

# Default target
.PHONY: all
all: lint test

# Install development dependencies
.PHONY: install-deps
install-deps:
	@echo "Installing development dependencies..."
	@if command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y yamllint shellcheck; \
	elif command -v yum >/dev/null 2>&1; then \
		sudo yum install -y yamllint ShellCheck; \
	elif command -v brew >/dev/null 2>&1; then \
		brew install yamllint shellcheck; \
	else \
		echo "Please install yamllint and shellcheck manually"; \
		exit 1; \
	fi

# Lint GitHub Actions YAML files
.PHONY: lint-yaml
lint-yaml:
	@echo "Linting GitHub Actions YAML files..."
	@if ! command -v yamllint >/dev/null 2>&1; then \
		echo "yamllint not found. Run 'make install-deps' first."; \
		exit 1; \
	fi
	@for file in .github/workflows/*.yml .github/workflows/*.yaml; do \
		if [ -f "$$file" ]; then \
			echo "Checking $$file..."; \
			yamllint -c .yamllint "$$file" || exit 1; \
		fi; \
	done
	@echo "✅ All YAML files passed linting"

# Lint shell scripts
.PHONY: lint-shell
lint-shell:
	@echo "Linting shell scripts..."
	@if ! command -v shellcheck >/dev/null 2>&1; then \
		echo "shellcheck not found. Run 'make install-deps' first."; \
		exit 1; \
	fi
	@find scripts -name "*.sh" -type f | while read -r script; do \
		echo "Checking $$script..."; \
		shellcheck "$$script" || exit 1; \
	done
	@echo "✅ All shell scripts passed linting"

# Run all linting
.PHONY: lint
lint: lint-yaml lint-shell
	@echo "✅ All linting checks passed"

# Run tests (placeholder - uses docker test script)
.PHONY: test
test:
	@echo "Running tests..."
	@if [ -f "tests/run-tests-docker.sh" ]; then \
		cd tests && ./run-tests-docker.sh --build-only ci-standard; \
	else \
		echo "Test script not found"; \
	fi

# Clean build artifacts
.PHONY: clean
clean:
	$(MAKE) -C src clean

# Install git pre-commit hook
.PHONY: install-hooks
install-hooks:
	@echo "Installing git pre-commit hook..."
	@mkdir -p .git/hooks
	@echo '#!/bin/bash' > .git/hooks/pre-commit
	@echo '# Git pre-commit hook for Matrix Creator kernel modules' >> .git/hooks/pre-commit
	@echo '# Automatically runs linting on GitHub Actions files when they change' >> .git/hooks/pre-commit
	@echo '' >> .git/hooks/pre-commit
	@echo 'set -e' >> .git/hooks/pre-commit
	@echo '' >> .git/hooks/pre-commit
	@echo '# Check if any GitHub Actions files are being committed' >> .git/hooks/pre-commit
	@echo 'if git diff --cached --name-only | grep -q "^\.github/workflows/.*\.ya*ml$$"; then' >> .git/hooks/pre-commit
	@echo '    echo "GitHub Actions files detected in commit. Running YAML linting..."' >> .git/hooks/pre-commit
	@echo '    make lint-yaml' >> .git/hooks/pre-commit
	@echo 'fi' >> .git/hooks/pre-commit
	@echo '' >> .git/hooks/pre-commit
	@echo '# Check if any shell scripts are being committed' >> .git/hooks/pre-commit
	@echo 'if git diff --cached --name-only | grep -q "\.sh$$"; then' >> .git/hooks/pre-commit
	@echo '    echo "Shell scripts detected in commit. Running shell linting..."' >> .git/hooks/pre-commit
	@echo '    make lint-shell' >> .git/hooks/pre-commit
	@echo 'fi' >> .git/hooks/pre-commit
	@echo '' >> .git/hooks/pre-commit
	@echo 'echo "✅ Pre-commit checks passed"' >> .git/hooks/pre-commit
	@chmod +x .git/hooks/pre-commit
	@echo "✅ Pre-commit hook installed"
	@echo "The hook will automatically run 'make lint-yaml' when GitHub Actions files change"
	@echo "and 'make lint-shell' when shell scripts change"

# Show help
.PHONY: help
help:
	@echo "Matrix Creator Kernel Modules - Development Targets"
	@echo ""
	@echo "Available targets:"
	@echo "  install-deps    Install development dependencies (yamllint, shellcheck)"
	@echo "  lint-yaml       Lint GitHub Actions YAML files"
	@echo "  lint-shell      Lint shell scripts"
	@echo "  lint            Run all linting checks"
	@echo "  test            Run tests"
	@echo "  clean           Clean build artifacts"
	@echo "  install-hooks   Install git pre-commit hooks"
	@echo "  help            Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make install-deps  # First time setup"
	@echo "  make lint          # Check all files"
	@echo "  make install-hooks # Setup git hooks"
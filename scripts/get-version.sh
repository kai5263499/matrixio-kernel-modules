#!/bin/bash

# Get the current version from git tags or generate one
# This script follows semantic versioning (MAJOR.MINOR.PATCH)

set -e

# Function to increment version
increment_version() {
    local version="$1"
    local level="$2"  # major, minor, patch
    
    # Split version into parts
    IFS='.' read -ra VERSION_PARTS <<< "$version"
    local major="${VERSION_PARTS[0]}"
    local minor="${VERSION_PARTS[1]}"
    local patch="${VERSION_PARTS[2]}"
    
    case "$level" in
        "major")
            echo "$((major + 1)).0.0"
            ;;
        "minor")
            echo "$major.$((minor + 1)).0"
            ;;
        "patch")
            echo "$major.$minor.$((patch + 1))"
            ;;
        *)
            echo "Error: Invalid increment level. Use major, minor, or patch" >&2
            exit 1
            ;;
    esac
}

# Get the latest version tag
get_latest_version() {
    # Get all version tags and sort them
    local latest_tag=$(git tag -l 'v*' | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -1)
    
    if [[ -n "$latest_tag" ]]; then
        # Remove 'v' prefix
        echo "${latest_tag#v}"
    else
        # No version tags found, start with 0.1.0
        echo "0.1.0"
    fi
}

# Generate next version
generate_next_version() {
    local current_version="$1"
    local increment_level="${2:-patch}"  # Default to patch increment
    
    increment_version "$current_version" "$increment_level"
}

# Main logic
case "${1:-}" in
    "--current"|"-c")
        # Get current version (latest tag or default)
        get_latest_version
        ;;
    "--next"|"-n")
        # Generate next version with optional increment level
        current=$(get_latest_version)
        generate_next_version "$current" "${2:-patch}"
        ;;
    "--help"|"-h")
        cat << EOF
Usage: $0 [OPTION] [INCREMENT_LEVEL]

Get or generate semantic version numbers from git tags.

OPTIONS:
    -c, --current          Get the current version (latest tag)
    -n, --next [LEVEL]     Generate next version (default: patch increment)
    -h, --help             Show this help

INCREMENT_LEVELS:
    major                  Increment major version (X.0.0)
    minor                  Increment minor version (X.Y.0)
    patch                  Increment patch version (X.Y.Z) [default]

EXAMPLES:
    $0 --current           # Get current version
    $0 --next              # Get next patch version
    $0 --next minor        # Get next minor version
    $0 --next major        # Get next major version

VERSION FORMAT:
    Tags should follow the pattern 'vX.Y.Z' (e.g., v0.3.1, v1.0.0)
    If no tags exist, starts with v0.1.0

EOF
        ;;
    "")
        # Default: show current version
        get_latest_version
        ;;
    *)
        echo "Error: Unknown option '$1'. Use --help for usage information." >&2
        exit 1
        ;;
esac
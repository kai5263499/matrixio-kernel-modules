#!/bin/bash

# Simple script to update the global version consistently across all files

set -e

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <new_version>"
    echo "Example: $0 0.4.0"
    exit 1
fi

NEW_VERSION="$1"

# Validate version format (basic semver)
if ! [[ "$NEW_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in format X.Y.Z (e.g., 0.4.0)"
    exit 1
fi

echo "Updating to version $NEW_VERSION..."

# Update VERSION file
echo "$NEW_VERSION" > VERSION

# Update debian changelogs
for changelog in debian-*/changelog; do
    if [[ -f "$changelog" ]]; then
        package_name=$(basename "$(dirname "$changelog")")
        # Update the first line of changelog
        sed -i.bak "1s/([^)]*)/($NEW_VERSION-1)/" "$changelog"
        rm -f "$changelog.bak"
        echo "Updated $changelog"
    fi
done

# Update original debian changelog if it exists
if [[ -f "debian/changelog" ]]; then
    sed -i.bak "1s/([^)]*)/($NEW_VERSION-1)/" "debian/changelog"
    rm -f "debian/changelog.bak"
    echo "Updated debian/changelog"
fi

echo "Version update complete!"
echo "Files updated:"
echo "  - VERSION"
echo "  - debian-*/changelog"
echo ""
echo "Next steps:"
echo "  1. Review changes: git diff"
echo "  2. Commit changes: git add -A && git commit -m 'chore: bump version to $NEW_VERSION'"
echo "  3. Push changes: git push"
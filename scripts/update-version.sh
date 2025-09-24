#!/bin/bash

# Script to create a new version tag and update changelogs accordingly
# Uses git tags as the source of truth for versioning

set -e

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Function to show usage
usage() {
    cat << EOF
Usage: $0 [INCREMENT_LEVEL] [OPTIONS]

Create a new version tag and update changelogs.

INCREMENT_LEVELS:
    major                  Increment major version (X.0.0)
    minor                  Increment minor version (X.Y.0)  
    patch                  Increment patch version (X.Y.Z) [default]

OPTIONS:
    --dry-run              Show what would be done without making changes
    --help                 Show this help message

EXAMPLES:
    $0                     # Increment patch version
    $0 minor               # Increment minor version
    $0 major               # Increment major version
    $0 patch --dry-run     # Show next patch version without creating

This script will:
1. Determine the next version based on existing git tags
2. Update debian changelogs with the new version
3. Create a git commit with the changelog updates
4. Create and push a version tag

EOF
}

# Parse command line arguments
INCREMENT_LEVEL="patch"
DRY_RUN=false

while [[ $# -gt 0 ]]; do
    case $1 in
        major|minor|patch)
            INCREMENT_LEVEL="$1"
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "Error: Unknown argument '$1'"
            usage
            exit 1
            ;;
    esac
done

# Make get-version.sh executable
chmod +x "$SCRIPT_DIR/get-version.sh"

# Get current and next versions
CURRENT_VERSION=$("$SCRIPT_DIR/get-version.sh" --current)
NEW_VERSION=$("$SCRIPT_DIR/get-version.sh" --next "$INCREMENT_LEVEL")

echo "Current version: $CURRENT_VERSION"
echo "New version: $NEW_VERSION"
echo "Increment level: $INCREMENT_LEVEL"

if [[ "$DRY_RUN" == "true" ]]; then
    echo ""
    echo "DRY RUN - No changes will be made"
    echo "Would create version tag: v$NEW_VERSION"
    exit 0
fi

echo ""
read -p "Proceed with version bump to $NEW_VERSION? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Version bump cancelled."
    exit 0
fi

echo "Creating version $NEW_VERSION..."

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

# Stage changelog changes
git add debian-*/changelog debian/changelog 2>/dev/null || true

# Create commit with changelog updates
if git diff --staged --quiet; then
    echo "No changelog changes to commit"
else
    git commit -m "chore: update changelogs for version $NEW_VERSION"
    echo "Committed changelog updates"
fi

# Create and push version tag
echo "Creating version tag v$NEW_VERSION..."
git tag -a "v$NEW_VERSION" -m "Release version $NEW_VERSION"

echo "Pushing tag to remote..."
git push origin "v$NEW_VERSION"

echo ""
echo "Version update complete!"
echo "Created tag: v$NEW_VERSION"
echo ""
echo "The GitHub Actions workflow will now:"
echo "  1. Build packages with version $NEW_VERSION"
echo "  2. Create a GitHub release"
echo "  3. Update the debian repository"
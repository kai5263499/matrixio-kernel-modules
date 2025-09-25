#!/bin/bash

# Semantic versioning helper script for Matrix Creator kernel modules
# This script handles version bumping based on commit messages or manual triggers

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CHANGELOG_FILE="$PROJECT_ROOT/debian/changelog"

# Default values
VERSION_TYPE=""
COMMIT_MESSAGE=""
DRY_RUN=0
FORCE=0

usage() {
    cat << EOF
Usage: $0 [OPTIONS] [VERSION_TYPE]

Semantic version management for Matrix Creator kernel modules.

VERSION_TYPE can be:
    major     - Increment major version (X.0.0) for breaking changes
    minor     - Increment minor version (x.Y.0) for new features
    patch     - Increment patch version (x.y.Z) for bug fixes
    auto      - Auto-detect from git commit messages (default)

OPTIONS:
    -m, --message MSG   Use specific commit message for auto-detection
    -n, --dry-run       Show what would be done without making changes
    -f, --force         Force version bump even if no trigger found
    -h, --help          Show this help message

COMMIT MESSAGE TRIGGERS:
    [version:major]     - Force major version bump
    [version:minor]     - Force minor version bump  
    [version:patch]     - Force patch version bump
    BREAKING CHANGE     - Force major version bump
    feat:, feature:     - Auto minor version bump
    fix:, bugfix:       - Auto patch version bump

EXAMPLES:
    $0                          # Auto-detect from latest commit
    $0 patch                    # Force patch version bump
    $0 --message "feat: new LED control" # Test with specific message
    $0 --dry-run auto           # Preview auto-detection

CURRENT VERSION: $(get_current_version)

EOF
}

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

get_current_version() {
    if [[ -f "$CHANGELOG_FILE" ]]; then
        dpkg-parsechangelog --show-field Version -l"$CHANGELOG_FILE" | cut -d'-' -f1
    else
        echo "0.1.0"
    fi
}

parse_version() {
    local version="$1"
    IFS='.' read -r major minor patch <<< "$version"
    echo "$major $minor $patch"
}

increment_version() {
    local current_version="$1"
    local increment_type="$2"
    
    read -r major minor patch <<< "$(parse_version "$current_version")"
    
    case "$increment_type" in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
        *)
            echo "Invalid increment type: $increment_type" >&2
            return 1
            ;;
    esac
    
    echo "$major.$minor.$patch"
}

detect_version_type_from_message() {
    local message="$1"
    
    # Explicit version triggers (highest priority)
    if [[ "$message" == *"[version:major]"* ]] || [[ "$message" == *"BREAKING CHANGE"* ]]; then
        echo "major"
        return 0
    elif [[ "$message" == *"[version:minor]"* ]]; then
        echo "minor"
        return 0
    elif [[ "$message" == *"[version:patch]"* ]]; then
        echo "patch"
        return 0
    fi
    
    # Conventional commit patterns
    if [[ "$message" == feat:* ]] || [[ "$message" == feature:* ]] || [[ "$message" == *"feat("* ]]; then
        echo "minor"
        return 0
    elif [[ "$message" == fix:* ]] || [[ "$message" == bugfix:* ]] || [[ "$message" == *"fix("* ]]; then
        echo "patch"
        return 0
    fi
    
    # No clear trigger found
    echo ""
    return 1
}

get_git_commit_message() {
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git log -1 --pretty=format:"%s%n%b" 2>/dev/null || echo ""
    else
        echo ""
    fi
}

update_changelog() {
    local new_version="$1"
    local change_description="$2"
    
    if [[ $DRY_RUN -eq 1 ]]; then
        log "DRY RUN: Would update changelog to version $new_version"
        return 0
    fi
    
    # Check if we have debchange available
    if ! command -v dch >/dev/null 2>&1; then
        log "WARNING: debchange (dch) not available. Install devscripts package."
        log "Manual changelog update required for version $new_version"
        return 1
    fi
    
    # Set environment for debchange
    export DEBEMAIL="${DEBEMAIL:-github-actions@github.com}"
    export DEBFULLNAME="${DEBFULLNAME:-Matrix Creator CI}"
    
    # Update changelog
    dch -v "$new_version-1" \
        --distribution unstable \
        "$change_description" || {
        log "ERROR: Failed to update changelog"
        return 1
    }
    
    log "Updated changelog to version $new_version"
    return 0
}

commit_version_change() {
    local new_version="$1"
    
    if [[ $DRY_RUN -eq 1 ]]; then
        log "DRY RUN: Would commit version change"
        return 0
    fi
    
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git add "$CHANGELOG_FILE"
        git commit -m "chore: bump version to $new_version [skip ci]" || {
            log "WARNING: Failed to commit version change"
            return 1
        }
        log "Committed version change to git"
    else
        log "Not in a git repository, skipping commit"
    fi
    
    return 0
}

main() {
    local current_version
    local new_version
    local version_type_detected
    local change_description
    
    current_version=$(get_current_version)
    log "Current version: $current_version"
    
    # Determine version increment type
    if [[ -n "$VERSION_TYPE" ]] && [[ "$VERSION_TYPE" != "auto" ]]; then
        # Manual version type specified
        version_type_detected="$VERSION_TYPE"
        change_description="Manual $VERSION_TYPE version bump"
        log "Manual version type: $version_type_detected"
    else
        # Auto-detect from commit message
        local message_to_check="$COMMIT_MESSAGE"
        if [[ -z "$message_to_check" ]]; then
            message_to_check=$(get_git_commit_message)
        fi
        
        if [[ -z "$message_to_check" ]]; then
            log "No commit message available for auto-detection"
            if [[ $FORCE -eq 0 ]]; then
                log "Use --force to bump patch version anyway"
                exit 1
            else
                version_type_detected="patch"
                change_description="Forced patch version bump"
            fi
        else
            log "Analyzing commit message: $(echo "$message_to_check" | head -1)"
            version_type_detected=$(detect_version_type_from_message "$message_to_check")
            
            if [[ -z "$version_type_detected" ]]; then
                log "No version trigger found in commit message"
                if [[ $FORCE -eq 0 ]]; then
                    log "Use --force to bump patch version anyway"
                    exit 0
                else
                    version_type_detected="patch"
                    change_description="Forced patch version bump"
                fi
            else
                change_description="Auto-detected $version_type_detected version bump: $(echo "$message_to_check" | head -1)"
            fi
        fi
    fi
    
    # Calculate new version
    new_version=$(increment_version "$current_version" "$version_type_detected")
    log "New version: $new_version (increment: $version_type_detected)"
    
    if [[ $DRY_RUN -eq 1 ]]; then
        log "DRY RUN: Version would change from $current_version to $new_version"
        exit 0
    fi
    
    # Update changelog
    if ! update_changelog "$new_version" "$change_description"; then
        log "ERROR: Failed to update changelog"
        exit 1
    fi
    
    # Commit changes if in git repo
    commit_version_change "$new_version"
    
    log "Version successfully bumped to $new_version"
    echo "$new_version"
}

# Parse command line arguments
ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -m|--message)
            COMMIT_MESSAGE="$2"
            shift 2
            ;;
        -n|--dry-run)
            DRY_RUN=1
            shift
            ;;
        -f|--force)
            FORCE=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

# Set positional arguments
set -- "${ARGS[@]}"

# Validate version type if provided
VERSION_TYPE="${1:-auto}"
case "$VERSION_TYPE" in
    major|minor|patch|auto)
        ;;
    *)
        echo "Invalid version type: $VERSION_TYPE" >&2
        echo "Valid types: major, minor, patch, auto" >&2
        exit 1
        ;;
esac

# Run main function
main
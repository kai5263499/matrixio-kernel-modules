#!/bin/bash

# Automatic version incrementation and tagging for main branch pushes
# This script determines the next version, creates a tag, and pushes it

set -e

echo "=== Automatic Version and Tag Creation ==="

# Only run on main/master branch pushes (not PRs or tags)
if [[ "$GITHUB_EVENT_NAME" != "push" ]]; then
  echo "Not a push event, skipping auto-versioning"
  exit 0
fi

if [[ "$GITHUB_REF" != "refs/heads/master" && "$GITHUB_REF" != "refs/heads/main" ]]; then
  echo "Not on main/master branch, skipping auto-versioning"
  exit 0
fi

# Don't auto-version if this push is already a tag
if [[ "$GITHUB_REF_TYPE" == "tag" ]]; then
  echo "Push is already a tag, skipping auto-versioning"
  exit 0
fi

# Make version script executable
chmod +x scripts/get-version.sh

# Get current version
CURRENT_VERSION=$(scripts/get-version.sh --current)
echo "Current version: $CURRENT_VERSION"

# Determine increment level from commit message
COMMIT_MESSAGE="$GITHUB_EVENT_HEAD_COMMIT_MESSAGE"
echo "Commit message: $COMMIT_MESSAGE"

# Check for version increment indicators in commit message
if echo "$COMMIT_MESSAGE" | grep -qE "\[version:major\]|BREAKING CHANGE"; then
  INCREMENT_LEVEL="major"
elif echo "$COMMIT_MESSAGE" | grep -qE "\[version:minor\]|feat:"; then
  INCREMENT_LEVEL="minor"
elif echo "$COMMIT_MESSAGE" | grep -qE "\[version:patch\]|fix:"; then
  INCREMENT_LEVEL="patch"
else
  # Default to patch increment for any other changes
  INCREMENT_LEVEL="patch"
fi

echo "Detected increment level: $INCREMENT_LEVEL"

# Generate next version
NEXT_VERSION=$(scripts/get-version.sh --next "$INCREMENT_LEVEL")
echo "Next version: $NEXT_VERSION"

# Check if this version already exists
if git tag -l "v$NEXT_VERSION" | grep -q "v$NEXT_VERSION"; then
  echo "Version v$NEXT_VERSION already exists, skipping tag creation"
  exit 0
fi

# Create and push the new tag
echo "Creating tag v$NEXT_VERSION"
git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"

git tag -a "v$NEXT_VERSION" -m "Automatic version bump to $NEXT_VERSION

Commit: $GITHUB_SHA
Message: $COMMIT_MESSAGE
Increment: $INCREMENT_LEVEL"

echo "Pushing tag v$NEXT_VERSION"
git push origin "v$NEXT_VERSION"

echo "Successfully created and pushed tag v$NEXT_VERSION"
echo "NEW_VERSION=$NEXT_VERSION" >> $GITHUB_ENV
echo "NEW_TAG=v$NEXT_VERSION" >> $GITHUB_ENV

echo "=== Auto-versioning complete ==="
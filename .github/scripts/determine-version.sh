#!/bin/bash

# Determine version for package builds
# This script handles semantic versioning based on the GitHub event type

set -e

echo "=== Determining package version ==="

# Make version script executable
chmod +x scripts/get-version.sh

# Determine version strategy based on event type
if [[ "$GITHUB_EVENT_NAME" == "push" && "$GITHUB_REF_TYPE" == "tag" ]]; then
  # For tag pushes, use the tag version
  TAG_VERSION="$GITHUB_REF_NAME"
  if [[ "$TAG_VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    VERSION="${TAG_VERSION#v}"  # Remove 'v' prefix
    echo "Using tag version: $VERSION"
  else
    echo "Error: Tag '$TAG_VERSION' doesn't follow semantic versioning (vX.Y.Z)"
    exit 1
  fi
elif [[ "$GITHUB_REF" == "refs/heads/master" || "$GITHUB_REF" == "refs/heads/main" ]]; then
  # For main/master branch, generate next patch version
  VERSION=$(scripts/get-version.sh --next patch)
  echo "Generated next version for main branch: $VERSION"
else
  # For other branches/PRs, use current version with branch suffix
  BASE_VERSION=$(scripts/get-version.sh --current)
  BRANCH_NAME=$(echo "$GITHUB_REF_NAME" | sed 's/[^a-zA-Z0-9]/-/g' | tr '[:upper:]' '[:lower:]')
  VERSION="${BASE_VERSION}-${BRANCH_NAME}-$GITHUB_RUN_NUMBER"
  echo "Generated development version: $VERSION"
fi

# Export version for use in GitHub Actions
echo "current_version=$VERSION" >> $GITHUB_OUTPUT
echo "new_version=$VERSION" >> $GITHUB_OUTPUT
echo "PKG_VER=$VERSION" >> $GITHUB_ENV

echo "Final version: $VERSION"
echo "## Building Version $VERSION" >> $GITHUB_STEP_SUMMARY
echo "🏗️ **Package Version**: $VERSION" >> $GITHUB_STEP_SUMMARY
echo "📦 **Event**: $GITHUB_EVENT_NAME" >> $GITHUB_STEP_SUMMARY
echo "🌿 **Branch**: $GITHUB_REF_NAME" >> $GITHUB_STEP_SUMMARY

echo "=== Version determination complete ==="
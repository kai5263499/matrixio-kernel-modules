#!/bin/bash

# Organize DEB packages by target and component
# This script processes downloaded .deb packages and organizes them into the repository structure

set -e

echo "=== Organizing DEB packages by target and component ==="

# Organize packages by target platform and component
for deb_file in *.deb; do
  if [[ -f "$deb_file" ]]; then
    echo "Processing: $deb_file"
    
    # Extract target, component, and architecture from filename
    # Format: matrixio-kernel-modules_TARGET-CODENAME-VERSION-COMPONENT_ARCH.deb
    if [[ "$deb_file" =~ matrixio-kernel-modules_([^-]+)-([^-]+)-([^-]+)-(main|unstable)_([^.]+)\.deb ]]; then
      target="${BASH_REMATCH[1]}"
      codename="${BASH_REMATCH[2]}"
      version="${BASH_REMATCH[3]}"
      component="${BASH_REMATCH[4]}"
      arch="${BASH_REMATCH[5]}"
      
      echo "  Target: $target, Codename: $codename, Version: $version, Component: $component, Arch: $arch"
      
      # Determine distribution based on component
      if [[ "$component" == "main" ]]; then
        dist="stable"
      else
        dist="unstable"
      fi
      
      # Copy to appropriate location
      cp "$deb_file" "debian-repo/pool/main/"
      
      echo "  Copied to debian-repo/pool/main/"
    else
      echo "  Warning: Filename doesn't match expected pattern, copying to pool anyway"
      cp "$deb_file" "debian-repo/pool/main/"
    fi
  fi
done

echo "=== Package organization complete ==="
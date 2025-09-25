#!/bin/bash

# Create package directory indexes
# This script generates HTML index files for the package directories

set -e

echo "=== Creating package directory indexes ==="

cd debian-repo

# Create index.html for pool directory
echo "Creating pool directory index..."
sed "s/REPO_OWNER/$GITHUB_REPOSITORY_OWNER/g; s/REPO_NAME/$GITHUB_EVENT_REPOSITORY_NAME/g" \
    ../.github/templates/pool-index.html > pool/index.html

# Create index.html for pool/main directory
echo "Creating pool/main directory index..."
cat ../.github/templates/package-list-header.html > pool/main/index.html

# List all .deb files and create entries
for deb_file in pool/main/*.deb; do
  if [[ -f "$deb_file" ]]; then
    filename=$(basename "$deb_file")
    filesize=$(ls -lh "$deb_file" | awk '{print $5}')
    
    # Extract package info
    if [[ "$filename" =~ matrixio-kernel-modules-([^_]+)_(.+)\.deb ]]; then
      package_variant="${BASH_REMATCH[1]}"
      version_info="${BASH_REMATCH[2]}"
      
      # Determine target description
      if [[ "$package_variant" == "bookworm" ]]; then
        target_desc="Modern ARM64 systems (Pi 5, updated Pi 4)"
        kernel_info="6.x kernels"
      elif [[ "$package_variant" == "buster" ]]; then
        target_desc="Legacy ARM32 systems (Pi 4 legacy)"
        kernel_info="5.10.x kernels"
      else
        target_desc="$package_variant systems"
        kernel_info="Multiple kernels"
      fi
      
      cat >> pool/main/index.html << EOF
        <div class="package-item">
            <div class="package-name">matrixio-kernel-modules-$package_variant</div>
            <div class="package-details">
                <strong>Target:</strong> $target_desc<br>
                <strong>Kernels:</strong> $kernel_info<br>
                <strong>File:</strong> $filename<br>
                <strong>Size:</strong> $filesize
            </div>
            <a href="$filename" class="download-link">Download Package</a>
        </div>
EOF
    fi
  fi
done

# Close the HTML
cat ../.github/templates/package-list-footer.html >> pool/main/index.html

echo "Created package index at pool/main/index.html"
echo "Available packages:"
ls -la pool/main/*.deb

echo "=== Package index creation complete ==="
#!/bin/bash

# Generate Debian repository structure and metadata
# This script is called by the GitHub Actions workflow

set -e

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <repo_directory>"
    echo "Example: $0 debian-repo"
    exit 1
fi

REPO_DIR="$1"

# Check if repo directory exists
if [[ ! -d "$REPO_DIR" ]]; then
    echo "Error: Repository directory '$REPO_DIR' does not exist"
    exit 1
fi

cd "$REPO_DIR"

echo "=== Generating Debian repository metadata ==="

# Debug: Show what packages we have
echo "=== Available packages in pool/main ==="
ls -la pool/main/ || echo "No pool/main directory found"
echo ""

# Generate Packages files for each distribution and architecture
echo "=== Generating Packages files ==="
for dist in stable unstable; do
    for arch in armhf arm64 all; do
        echo "Generating Packages file for $dist/$arch"
        
        # Create directory if it doesn't exist
        mkdir -p "dists/$dist/main/binary-$arch"
        
        # Generate Packages file
        if dpkg-scanpackages \
            --arch "$arch" \
            pool/main \
            /dev/null \
            > "dists/$dist/main/binary-$arch/Packages" 2>/dev/null; then
            
            # Compress Packages file if not empty
            if [[ -s "dists/$dist/main/binary-$arch/Packages" ]]; then
                gzip -c "dists/$dist/main/binary-$arch/Packages" > "dists/$dist/main/binary-$arch/Packages.gz"
                package_count=$(wc -l < "dists/$dist/main/binary-$arch/Packages")
                echo "Generated: dists/$dist/main/binary-$arch/Packages ($package_count packages)"
            else
                echo "No packages found for $dist/$arch"
                # Create empty files to avoid APT errors
                touch "dists/$dist/main/binary-$arch/Packages"
                gzip -c "dists/$dist/main/binary-$arch/Packages" > "dists/$dist/main/binary-$arch/Packages.gz"
            fi
        else
            echo "Failed to generate packages for $dist/$arch"
            # Create empty files to avoid APT errors
            touch "dists/$dist/main/binary-$arch/Packages"
            gzip -c "dists/$dist/main/binary-$arch/Packages" > "dists/$dist/main/binary-$arch/Packages.gz"
        fi
    done
done

# Generate Release files for each distribution
echo ""
echo "=== Generating Release files ==="
for dist in stable unstable; do
    echo "Generating Release file for $dist"
    release_file="dists/$dist/Release"
    
    # Create Release file header
    cat > "$release_file" << EOF
Origin: Matrix Creator
Label: Matrix Creator Kernel Modules
Suite: $dist
Codename: $dist
Version: 1.0
Components: main
Architectures: armhf arm64 all
Description: Matrix Creator HAT kernel modules for Raspberry Pi systems
Date: $(date -R)
EOF
    
    # Add hash sections for Packages files
    if ls "dists/$dist/main/binary-"*/Packages* >/dev/null 2>&1; then
        echo "MD5Sum:" >> "$release_file"
        find "dists/$dist" -name "Packages*" -type f | while read file; do
            relpath="${file#dists/$dist/}"
            size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
            md5hash=$(md5sum "$file" | cut -d' ' -f1)
            printf " %s %8d %s\n" "$md5hash" "$size" "$relpath" >> "$release_file"
        done
        
        echo "SHA1:" >> "$release_file"
        find "dists/$dist" -name "Packages*" -type f | while read file; do
            relpath="${file#dists/$dist/}"
            size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
            sha1hash=$(sha1sum "$file" | cut -d' ' -f1)
            printf " %s %8d %s\n" "$sha1hash" "$size" "$relpath" >> "$release_file"
        done
        
        echo "SHA256:" >> "$release_file"
        find "dists/$dist" -name "Packages*" -type f | while read file; do
            relpath="${file#dists/$dist/}"
            size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file")
            sha256hash=$(sha256sum "$file" | cut -d' ' -f1)
            printf " %s %8d %s\n" "$sha256hash" "$size" "$relpath" >> "$release_file"
        done
    fi
    
    echo "Generated Release file for $dist"
    echo "Contents:"
    cat "$release_file"
    echo ""
done

echo "=== Repository metadata generation complete ==="
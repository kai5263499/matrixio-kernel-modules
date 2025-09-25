#!/bin/bash

# Generate repository summary for GitHub Actions summary
# This script creates a comprehensive summary of the published repository

set -e

echo "=== Generating repository summary ==="

cat >> $GITHUB_STEP_SUMMARY << 'EOF'
## Debian Repository Published

🎉 **Repository URL:** https://REPO_OWNER.github.io/REPO_NAME/

### Quick Setup Commands
```bash
# Add repository
echo "deb [trusted=yes] https://REPO_OWNER.github.io/REPO_NAME/ stable main" | sudo tee /etc/apt/sources.list.d/matrixio.list

# Update and install
sudo apt update
sudo apt install matrixio-kernel-modules
```

### Packages in Repository
| Distribution | Architecture | Packages |
|-------------|--------------|----------|
EOF

# Replace placeholders with actual values
sed -i "s/REPO_OWNER/$GITHUB_REPOSITORY_OWNER/g; s/REPO_NAME/$GITHUB_EVENT_REPOSITORY_NAME/g" $GITHUB_STEP_SUMMARY

cd debian-repo
for dist in stable unstable; do
  for arch in armhf arm64 all; do
    if [[ -f "dists/$dist/main/binary-$arch/Packages" ]]; then
      count=$(grep -c "^Package:" "dists/$dist/main/binary-$arch/Packages" || echo "0")
      echo "| $dist | $arch | $count |" >> $GITHUB_STEP_SUMMARY
    fi
  done
done

echo "=== Repository summary generated ==="
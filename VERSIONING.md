# Semantic Versioning Guide

This project uses semantic versioning (SemVer) starting from version 0.1.0. Version bumps are automated based on commit message patterns.

## Version Format

Versions follow the format: `MAJOR.MINOR.PATCH`

- **MAJOR**: Incremented for breaking changes that affect API compatibility
- **MINOR**: Incremented for new features that maintain backward compatibility  
- **PATCH**: Incremented for bug fixes and patches

## Automatic Version Bumping

### Commit Message Triggers

The CI/CD system automatically detects version changes based on commit messages:

#### Explicit Version Control (Highest Priority)
- `[version:major]` - Forces major version increment
- `[version:minor]` - Forces minor version increment
- `[version:patch]` - Forces patch version increment
- `BREAKING CHANGE` - Forces major version increment

#### Conventional Commit Patterns
- `feat:` or `feature:` - Auto minor version bump
- `fix:` or `bugfix:` - Auto patch version bump

### Examples

```bash
# Major version bump (1.2.3 → 2.0.0)
git commit -m "feat: new SPI interface [version:major]"
git commit -m "BREAKING CHANGE: remove deprecated GPIO functions"

# Minor version bump (1.2.3 → 1.3.0)  
git commit -m "feat: add environmental sensor support"
git commit -m "feature: implement fuzzing tests [version:minor]"

# Patch version bump (1.2.3 → 1.2.4)
git commit -m "fix: resolve SPI timeout issue"
git commit -m "bugfix: correct LED buffer alignment"
git commit -m "docs: update installation guide [version:patch]"
```

## Manual Version Management

### Using the Version Bump Script

```bash
# Auto-detect from latest commit
./scripts/version-bump.sh

# Force specific version type
./scripts/version-bump.sh major
./scripts/version-bump.sh minor  
./scripts/version-bump.sh patch

# Test with specific commit message
./scripts/version-bump.sh --message "feat: new LED control"

# Preview changes without applying
./scripts/version-bump.sh --dry-run
```

### Manual Changelog Updates

If you need to manually update the version:

```bash
# Install debchange if not available
sudo apt-get install devscripts

# Update changelog
export DEBEMAIL="your.email@example.com"
export DEBFULLNAME="Your Name"
dch -v "0.3.0-1" --distribution unstable "Description of changes"
```

## CI/CD Integration

### GitHub Actions Workflow

The automated workflow follows this sequence:

1. **Test Phase**: Run KUnit tests on all target platforms
2. **Build Phase**: Cross-compile kernel modules (only if tests pass)
3. **Package Phase**: Create Debian packages (only if build succeeds)
4. **Version Management**: Auto-increment version based on commit messages
5. **Release**: Create GitHub releases for tagged versions

### Version Triggers in CI

- **Push to main/master**: Auto-increment patch version if commit contains triggers
- **Pull Requests**: No version changes, but tests still run
- **Tagged Releases**: Use existing tag version, create GitHub release

### Workflow Files

- `.github/workflows/test-build-package.yml` - Main sequential workflow
- `.github/workflows/build-and-package.yml` - Legacy build-only workflow (kept for reference)

## Target Platform Versions

The version compatibility matrix:

| Platform | Kernel Version | Architecture | Status |
|----------|----------------|--------------|---------|
| Pi 4 Legacy | 5.10.103-v7l+ | ARM32 | ✅ Supported |
| Pi 5 Latest | 6.12.34+rpt-rpi-2712 | ARM64 | ✅ Supported |  
| CI Standard | 6.1.70-rpi-v8 | ARM64 | ✅ Tested |

## Version History

Current version: **0.2.5** (as of last changelog entry)

Next planned version: **0.1.0** (reset for new semantic versioning system)

### Migration from Legacy Versioning

The project is transitioning from the existing 0.2.x series to a clean 0.1.0 start with the new semantic versioning system. This allows for:

- Clean semantic version tracking
- Automated CI/CD version management
- Clear compatibility guarantees
- Proper pre-1.0 development versioning

## Release Process

### Development Releases (0.x.x)

- All releases before 1.0.0 are considered development releases
- Breaking changes may occur between minor versions
- Patch versions should maintain compatibility within the same minor series

### Stable Releases (1.x.x+)

- Major versions (1.0.0+) follow strict semantic versioning
- Breaking changes only in major versions
- Backward compatibility maintained in minor and patch versions

### Release Checklist

1. Ensure all tests pass locally: `./tests/run-tests-docker.sh`
2. Update documentation if needed
3. Commit with appropriate version trigger
4. Push to main branch
5. CI automatically handles version bump and packaging
6. Create git tag for releases: `git tag v0.1.0 && git push origin v0.1.0`
7. GitHub Actions creates release automatically

## Troubleshooting

### Version Not Incrementing

- Check commit message contains valid triggers
- Verify GitHub Actions has write permissions for the repository
- Ensure `devscripts` package is available in CI environment

### Manual Version Fix

If automated versioning fails:

```bash
# Fix version manually
./scripts/version-bump.sh --force patch
git push origin main
```

### Rollback Version

```bash
# Edit debian/changelog manually
nano debian/changelog

# Commit the fix
git add debian/changelog
git commit -m "fix: correct version in changelog [version:patch]"
```
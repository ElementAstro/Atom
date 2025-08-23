# Atom Library CI/CD Guide

This guide covers the comprehensive Continuous Integration and Continuous Deployment (CI/CD) system for the Atom library.

## Table of Contents

- [Overview](#overview)
- [GitHub Actions Workflows](#github-actions-workflows)
- [Workflow Configuration](#workflow-configuration)
- [Local Development](#local-development)
- [Release Process](#release-process)
- [Monitoring and Maintenance](#monitoring-and-maintenance)

## Overview

The Atom library uses GitHub Actions for CI/CD with the following key features:

- **Multi-platform builds**: Linux, Windows, macOS
- **Multiple build systems**: CMake and XMake
- **Code quality checks**: Static analysis, formatting, security
- **Automated testing**: Unit tests, integration tests, memory safety
- **Documentation generation**: Doxygen, GitHub Pages
- **Package distribution**: Binary packages, Python wheels, vcpkg ports
- **Dependency management**: Automated updates and security scanning

## GitHub Actions Workflows

### 1. Continuous Integration (`ci.yml`)

**Triggers**: Push to main/develop, Pull requests, Manual dispatch

**Jobs**:
- **Code Quality**: Static analysis, formatting checks, linting
- **Build Matrix**: Multi-platform builds with different configurations
- **Python Bindings**: Test Python bindings across Python versions
- **Security Scanning**: CodeQL analysis for security vulnerabilities
- **Performance Benchmarks**: Automated performance testing

**Configuration Example**:
```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest, macos-latest]
    build_system: [cmake, xmake]
    config: [Debug, Release]
```

### 2. Release Workflow (`release.yml`)

**Triggers**: Git tags (v*), Manual dispatch

**Jobs**:
- **Build Release**: Create optimized builds for all platforms
- **Python Wheels**: Build Python wheels using cibuildwheel
- **Documentation**: Generate and deploy documentation
- **GitHub Release**: Create GitHub release with assets
- **Package Distribution**: Deploy to PyPI, create vcpkg port

**Artifacts Created**:
- Binary packages (tar.gz, zip)
- Python wheels (.whl)
- Documentation (HTML)
- vcpkg port files
- Checksums and signatures

### 3. Code Quality (`code-quality.yml`)

**Triggers**: Push, Pull requests, Weekly schedule, Manual dispatch

**Analysis Tools**:
- **Static Analysis**: cppcheck, clang-tidy, cpplint
- **Security**: CodeQL, Semgrep
- **Memory Safety**: AddressSanitizer, MemorySanitizer, Valgrind
- **Performance**: Profiling, coverage analysis
- **Documentation**: Completeness checks, Doxygen warnings

### 4. Dependency Updates (`dependency-update.yml`)

**Triggers**: Weekly schedule, Manual dispatch

**Update Types**:
- **vcpkg Baseline**: Update to latest vcpkg commit
- **Git Submodules**: Update submodule references
- **Python Dependencies**: Update Python package versions
- **Security Scanning**: Check for vulnerabilities
- **License Compliance**: Verify license compatibility

## Workflow Configuration

### Environment Variables

```yaml
env:
  BUILD_TYPE: Release
  VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"
  PYTHONPATH: ${{ github.workspace }}/build/python
```

### Secrets Required

- `GITHUB_TOKEN`: Automatic token for GitHub API access
- `PYPI_API_TOKEN`: For publishing Python packages
- `CODECOV_TOKEN`: For code coverage reporting (optional)

### Matrix Strategy

The CI system uses matrix builds to test multiple configurations:

```yaml
strategy:
  fail-fast: false
  matrix:
    os: [ubuntu-latest, windows-latest, macos-latest]
    build_system: [cmake, xmake]
    config: [Debug, Release]
    include:
      - os: ubuntu-latest
        triplet: x64-linux
      - os: windows-latest
        triplet: x64-windows
      - os: macos-latest
        triplet: x64-osx
```

### Caching Strategy

Efficient caching reduces build times:

```yaml
- name: Cache vcpkg
  uses: actions/cache@v3
  with:
    path: |
      ${{ github.workspace }}/vcpkg
      !${{ github.workspace }}/vcpkg/buildtrees
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}

- name: Cache build
  uses: actions/cache@v3
  with:
    path: build
    key: build-${{ runner.os }}-${{ matrix.config }}-${{ hashFiles('CMakeLists.txt') }}
```

## Local Development

### Pre-commit Hooks

Install pre-commit hooks for local quality checks:

```bash
# Install pre-commit
pip install pre-commit

# Install hooks
pre-commit install

# Run manually
pre-commit run --all-files
```

### Local CI Simulation

Use `act` to run GitHub Actions locally:

```bash
# Install act
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Run CI workflow
act -j build

# Run specific job
act -j code-quality
```

### Quality Checks

Run the same quality checks locally:

```bash
# Static analysis
cppcheck --enable=all atom/

# Code formatting
find atom/ -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Build with sanitizers
cmake -B build-debug \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined"
```

## Release Process

### Automated Release

1. **Version Bump**: Use version management script
   ```bash
   ./scripts/version-manager.sh release minor
   ```

2. **Push Tag**: The release workflow triggers automatically
   ```bash
   git push origin v1.2.0
   ```

3. **Monitor**: Check GitHub Actions for build status

### Manual Release

1. **Prepare Release**:
   ```bash
   # Update version
   ./scripts/version-manager.sh set 1.2.0

   # Generate changelog
   ./scripts/version-manager.sh changelog

   # Commit changes
   git add -A
   git commit -m "Release version 1.2.0"
   git tag -a v1.2.0 -m "Release version 1.2.0"
   ```

2. **Trigger Workflow**:
   ```bash
   git push origin main
   git push origin v1.2.0
   ```

### Release Checklist

- [ ] Version updated in all files
- [ ] Changelog generated
- [ ] Tests passing
- [ ] Documentation updated
- [ ] Security scan clean
- [ ] Performance benchmarks acceptable

## Monitoring and Maintenance

### Workflow Status

Monitor workflow status through:
- GitHub Actions tab
- Status badges in README
- Email notifications (configure in GitHub settings)

### Failure Handling

Common failure scenarios and solutions:

#### Build Failures
```bash
# Check build logs
# Fix compilation errors
# Update dependencies if needed
```

#### Test Failures
```bash
# Review test output
# Fix failing tests
# Update test expectations if needed
```

#### Dependency Issues
```bash
# Update vcpkg baseline
# Resolve version conflicts
# Update system dependencies
```

### Performance Monitoring

Track build performance:
- Build times across platforms
- Test execution times
- Package sizes
- Memory usage during builds

### Security Monitoring

Regular security practices:
- CodeQL analysis results
- Dependency vulnerability scans
- Secret scanning
- License compliance checks

### Maintenance Tasks

#### Weekly
- Review dependency update PRs
- Check security scan results
- Monitor build performance
- Update documentation

#### Monthly
- Review and update CI configuration
- Analyze build metrics
- Update development tools
- Security audit

#### Quarterly
- Major dependency updates
- CI/CD system improvements
- Performance optimization
- Documentation overhaul

## Best Practices

### Workflow Design
- Keep workflows focused and modular
- Use matrix builds for comprehensive testing
- Implement proper caching strategies
- Handle failures gracefully

### Security
- Use minimal required permissions
- Store secrets securely
- Regularly update actions
- Monitor for vulnerabilities

### Performance
- Optimize build times with caching
- Use parallel builds
- Minimize artifact sizes
- Monitor resource usage

### Maintenance
- Keep workflows up to date
- Document configuration changes
- Monitor for deprecated features
- Regular testing of CI/CD system

## Troubleshooting

### Common Issues

#### Workflow Not Triggering
- Check trigger conditions
- Verify branch protection rules
- Check repository permissions

#### Build Timeouts
- Increase timeout values
- Optimize build process
- Use better caching

#### Artifact Upload Failures
- Check artifact sizes
- Verify paths exist
- Check permissions

### Getting Help

- Check GitHub Actions documentation
- Review workflow logs
- Use GitHub Community discussions
- Contact maintainers

For more information, see:
- [Build Guide](BUILD_GUIDE.md)
- [Distribution Guide](DISTRIBUTION_GUIDE.md)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)

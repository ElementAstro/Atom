# Atom Library Deployment Guide

This guide covers deployment strategies, environment setup, and production considerations for the Atom library.

## Table of Contents

- [Overview](#overview)
- [Deployment Environments](#deployment-environments)
- [Installation Strategies](#installation-strategies)
- [Configuration Management](#configuration-management)
- [Monitoring and Maintenance](#monitoring-and-maintenance)
- [Security Considerations](#security-considerations)
- [Troubleshooting](#troubleshooting)

## Overview

The Atom library can be deployed in various environments and configurations:

- **Development environments**: Local development and testing
- **Staging environments**: Pre-production testing and validation
- **Production environments**: Live systems and applications
- **Container environments**: Docker, Kubernetes, cloud platforms
- **Embedded systems**: Resource-constrained environments

## Deployment Environments

### Development Environment

**Purpose**: Local development, debugging, testing
**Configuration**: Debug builds, all features enabled

```bash
# Setup development environment
./build.sh --debug --examples --tests --python --docs --install-deps

# Install in development mode
cmake --install build --prefix ~/.local

# Python development install
pip install -e .
```

**Environment Variables**:

```bash
export CMAKE_PREFIX_PATH=~/.local:$CMAKE_PREFIX_PATH
export LD_LIBRARY_PATH=~/.local/lib:$LD_LIBRARY_PATH
export PYTHONPATH=~/.local/lib/python3.11/site-packages:$PYTHONPATH
```

### Staging Environment

**Purpose**: Pre-production testing, integration validation
**Configuration**: Release builds, production-like setup

```bash
# Build for staging
./build.sh --release --examples --tests --package

# Install system-wide
sudo cmake --install build --prefix /usr/local

# Verify installation
atom-info --version
python -c "import atom; print(atom.__version__)"
```

### Production Environment

**Purpose**: Live applications, performance-critical systems
**Configuration**: Optimized builds, minimal features

```bash
# Production build
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/atom \
    -DATOM_BUILD_EXAMPLES=OFF \
    -DATOM_BUILD_TESTS=OFF \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

cmake --build build --parallel
sudo cmake --install build
```

## Installation Strategies

### System-Wide Installation

**Advantages**: Available to all users, standard locations
**Disadvantages**: Requires admin privileges, potential conflicts

```bash
# Install to /usr/local (default)
sudo cmake --install build

# Install to /opt
sudo cmake --install build --prefix /opt/atom

# Update system configuration
echo '/opt/atom/lib' | sudo tee /etc/ld.so.conf.d/atom.conf
sudo ldconfig
```

### User-Local Installation

**Advantages**: No admin privileges, isolated environment
**Disadvantages**: Only available to single user

```bash
# Install to user directory
cmake --install build --prefix ~/.local

# Update user environment
echo 'export PATH=~/.local/bin:$PATH' >> ~/.bashrc
echo 'export CMAKE_PREFIX_PATH=~/.local:$CMAKE_PREFIX_PATH' >> ~/.bashrc
```

### Virtual Environment

**Advantages**: Isolated dependencies, version control
**Disadvantages**: Additional complexity

```bash
# Create virtual environment
python -m venv atom-env
source atom-env/bin/activate

# Install Python bindings
pip install atom

# C++ environment with modules
export ATOM_ROOT=$PWD/atom-env
cmake --install build --prefix $ATOM_ROOT
```

### Container Deployment

**Advantages**: Consistent environment, easy scaling
**Disadvantages**: Container overhead

```dockerfile
# Multi-stage Dockerfile
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build \
    libssl-dev zlib1g-dev libsqlite3-dev

COPY . /src
WORKDIR /src
RUN ./build.sh --release --clean
RUN cmake --install build --prefix /opt/atom

FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y \
    libssl3 zlib1g libsqlite3-0 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/atom /opt/atom
ENV PATH=/opt/atom/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/atom/lib:$LD_LIBRARY_PATH
```

### Package Manager Deployment

**Advantages**: Automatic dependency management
**Disadvantages**: Limited to supported platforms

```bash
# vcpkg
vcpkg install atom
vcpkg integrate install

# Conan
conan install atom/1.0.0@
conan imports

# System package managers
sudo apt-get install libatom-dev  # Ubuntu/Debian
sudo dnf install atom-devel       # Fedora
brew install atom                 # macOS
```

## Configuration Management

### Build Configuration

**CMake Presets** (`CMakePresets.json`):

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "production",
      "displayName": "Production Build",
      "binaryDir": "build-prod",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "ATOM_BUILD_EXAMPLES": "OFF",
        "ATOM_BUILD_TESTS": "OFF",
        "CMAKE_INTERPROCEDURAL_OPTIMIZATION": "ON"
      }
    }
  ]
}
```

**Environment Configuration**:

```bash
# Production environment
export ATOM_LOG_LEVEL=WARNING
export ATOM_CONFIG_PATH=/etc/atom
export ATOM_DATA_PATH=/var/lib/atom
export ATOM_CACHE_PATH=/var/cache/atom
```

### Runtime Configuration

**Configuration Files**:

```yaml
# /etc/atom/config.yaml
logging:
  level: INFO
  file: /var/log/atom/atom.log

database:
  path: /var/lib/atom/data.db
  pool_size: 10

network:
  timeout: 30
  retry_count: 3
```

**Environment Variables**:

```bash
# Application settings
ATOM_CONFIG_FILE=/etc/atom/config.yaml
ATOM_LOG_LEVEL=INFO
ATOM_DEBUG=false

# Performance tuning
ATOM_THREAD_POOL_SIZE=8
ATOM_MEMORY_LIMIT=1GB
ATOM_CACHE_SIZE=256MB
```

## Monitoring and Maintenance

### Health Checks

```bash
# System health check
atom-info --health

# Library version check
atom-info --version --verbose

# Dependency check
ldd /opt/atom/lib/libatom.so
```

### Logging Configuration

```cpp
// Application logging setup
#include <atom/log/logger.hpp>

atom::log::Logger::configure({
    .level = atom::log::Level::INFO,
    .file = "/var/log/myapp/app.log",
    .rotation = atom::log::Rotation::DAILY,
    .max_size = "100MB"
});
```

### Performance Monitoring

```bash
# Monitor resource usage
top -p $(pgrep -f atom)
htop -p $(pgrep -f atom)

# Memory profiling
valgrind --tool=massif ./my-atom-app

# Performance profiling
perf record -g ./my-atom-app
perf report
```

### Update Management

```bash
# Check for updates
./scripts/version-manager.sh current

# Update to new version
wget https://github.com/ElementAstro/Atom/releases/download/v1.1.0/atom-1.1.0-linux-x64.tar.gz
tar -xzf atom-1.1.0-linux-x64.tar.gz
sudo cp -r atom-1.1.0-linux-x64/* /opt/atom/

# Restart services
sudo systemctl restart my-atom-service
```

## Security Considerations

### File Permissions

```bash
# Secure installation permissions
sudo chown -R root:root /opt/atom
sudo chmod -R 755 /opt/atom
sudo chmod 644 /opt/atom/lib/*
```

### Network Security

```cpp
// Secure network configuration
#include <atom/web/client.hpp>

atom::web::Client client({
    .verify_ssl = true,
    .timeout = std::chrono::seconds(30),
    .max_redirects = 3
});
```

### Data Protection

```bash
# Encrypt sensitive data
export ATOM_ENCRYPTION_KEY=$(openssl rand -base64 32)

# Secure configuration files
sudo chmod 600 /etc/atom/config.yaml
sudo chown root:atom /etc/atom/config.yaml
```

### Access Control

```bash
# Create atom user group
sudo groupadd atom
sudo usermod -a -G atom $USER

# Set group permissions
sudo chgrp -R atom /var/lib/atom
sudo chmod -R 750 /var/lib/atom
```

## Service Integration

### Systemd Service

```ini
# /etc/systemd/system/atom-service.service
[Unit]
Description=Atom Service
After=network.target

[Service]
Type=simple
User=atom
Group=atom
ExecStart=/opt/atom/bin/atom-service
Restart=always
RestartSec=10
Environment=ATOM_CONFIG_FILE=/etc/atom/config.yaml
Environment=ATOM_LOG_LEVEL=INFO

[Install]
WantedBy=multi-user.target
```

```bash
# Enable and start service
sudo systemctl enable atom-service
sudo systemctl start atom-service
sudo systemctl status atom-service
```

### Docker Compose

```yaml
# docker-compose.yml
version: '3.8'
services:
  atom-app:
    image: atom/atom:1.0.0
    ports:
      - "8080:8080"
    environment:
      - ATOM_LOG_LEVEL=INFO
      - ATOM_CONFIG_FILE=/app/config.yaml
    volumes:
      - ./config:/app/config:ro
      - atom-data:/var/lib/atom
    restart: unless-stopped

volumes:
  atom-data:
```

### Kubernetes Deployment

```yaml
# atom-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: atom-app
spec:
  replicas: 3
  selector:
    matchLabels:
      app: atom-app
  template:
    metadata:
      labels:
        app: atom-app
    spec:
      containers:
      - name: atom-app
        image: atom/atom:1.0.0
        ports:
        - containerPort: 8080
        env:
        - name: ATOM_LOG_LEVEL
          value: "INFO"
        resources:
          requests:
            memory: "256Mi"
            cpu: "250m"
          limits:
            memory: "512Mi"
            cpu: "500m"
```

## Troubleshooting

### Common Issues

#### Library Not Found

```bash
# Check library path
echo $LD_LIBRARY_PATH
ldd /path/to/your/app

# Fix library path
export LD_LIBRARY_PATH=/opt/atom/lib:$LD_LIBRARY_PATH
sudo ldconfig
```

#### Version Conflicts

```bash
# Check installed versions
atom-info --version
pkg-config --modversion atom

# Remove conflicting versions
sudo apt-get remove libatom-dev
sudo rm -rf /usr/local/include/atom
```

#### Permission Errors

```bash
# Fix permissions
sudo chown -R $USER:$USER ~/.local
sudo chmod -R 755 /opt/atom
```

#### Performance Issues

```bash
# Check resource usage
htop
iostat -x 1
free -h

# Optimize configuration
export ATOM_THREAD_POOL_SIZE=$(nproc)
export ATOM_MEMORY_LIMIT=2GB
```

### Diagnostic Tools

```bash
# System information
atom-info --system
uname -a
lscpu
free -h

# Library diagnostics
ldd /opt/atom/lib/libatom.so
nm -D /opt/atom/lib/libatom.so | grep atom

# Network diagnostics
netstat -tlnp | grep atom
ss -tlnp | grep atom
```

### Log Analysis

```bash
# View logs
tail -f /var/log/atom/atom.log
journalctl -u atom-service -f

# Search for errors
grep -i error /var/log/atom/atom.log
grep -i warning /var/log/atom/atom.log
```

## Best Practices

### Deployment

- Use configuration management tools
- Implement blue-green deployments
- Test in staging before production
- Monitor deployment metrics

### Security

- Regular security updates
- Principle of least privilege
- Encrypt sensitive data
- Regular security audits

### Performance

- Monitor resource usage
- Optimize for target hardware
- Use appropriate build flags
- Profile critical paths

### Maintenance

- Regular backups
- Automated monitoring
- Update procedures
- Disaster recovery plans

For more information, see:

- [Build Guide](BUILD_GUIDE.md)
- [CI/CD Guide](CI_CD_GUIDE.md)
- [Distribution Guide](DISTRIBUTION_GUIDE.md)

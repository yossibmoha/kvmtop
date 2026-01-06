# Installation Guide

This guide covers different ways to install and run kvmtop on your Linux system.

## Quick Start (Pre-compiled Binary)

The easiest way to run kvmtop is to download the latest static binary. It works on **any** x86_64 Linux distribution (Debian, Ubuntu, CentOS, RHEL, Alpine, Proxmox).

```bash
# Download the latest release
curl -L -o kvmtop https://github.com/yohaya/kvmtop/releases/download/latest/kvmtop-static-linux-amd64

# Make it executable
chmod +x kvmtop

# Run (sudo recommended for full I/O statistics)
sudo ./kvmtop
```

> **Note:** `sudo` is highly recommended to view Disk I/O statistics, which are restricted by the kernel to root users.

## Building from Source

If you prefer to build kvmtop yourself:

### Prerequisites

- `gcc` compiler
- `make` build tool
- Git (for cloning the repository)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/yohaya/kvmtop.git
cd kvmtop

# Build the binary
make

# The binary will be in the build/ directory
sudo ./build/kvmtop
```

### Build Options

You can customize the build with environment variables:

```bash
# Build with a specific version
make VERSION=v1.2.3

# Build with custom compiler flags
make CFLAGS="-Wall -Wextra -O3"

# Clean build artifacts
make clean
```

## System-Wide Installation

To install kvmtop system-wide:

```bash
# Copy to /usr/local/bin (requires root)
sudo cp build/kvmtop /usr/local/bin/

# Now you can run it from anywhere
sudo kvmtop
```

Or add to `/usr/bin` for package-managed systems:

```bash
sudo cp build/kvmtop /usr/bin/
```

## Running Without Root

While kvmtop can run without root privileges, many I/O statistics will be unavailable:

```bash
# Running without sudo
./kvmtop
```

You'll see a warning:
```
Warning: Not running as root. IO stats will be unavailable for other users' processes.
```

### What You'll Miss

Without root access, kvmtop cannot read:
- `/proc/[pid]/io` files (read/write bytes, I/O operations)
- I/O wait times and latency metrics
- Full disk statistics

### Recommendation

For production monitoring, always run kvmtop with `sudo` to get complete metrics.

## Verifying Installation

To verify kvmtop is installed correctly:

```bash
# Check version
kvmtop --version

# Should output something like:
# kvmtop v1.0.1-dev

# Test run (Ctrl+C to exit)
sudo kvmtop --interval 2
```

## Platform-Specific Notes

### Proxmox VE

kvmtop works perfectly on Proxmox VE. It will automatically detect and name your VMs:

```bash
# SSH into your Proxmox host
ssh root@proxmox-host

# Download and run
curl -L -o kvmtop https://github.com/yohaya/kvmtop/releases/download/latest/kvmtop-static-linux-amd64
chmod +x kvmtop
./kvmtop
```

### RHEL/CentOS/Rocky Linux

The static binary works on RHEL-based systems without any additional packages:

```bash
sudo ./kvmtop
```

### Ubuntu/Debian

Works out of the box on all modern Ubuntu and Debian releases:

```bash
sudo ./kvmtop
```

### Alpine Linux

The static binary is compatible with Alpine's musl libc:

```bash
sudo ./kvmtop
```

## Uninstallation

To remove kvmtop:

```bash
# If installed to /usr/local/bin
sudo rm /usr/local/bin/kvmtop

# Or if installed to /usr/bin
sudo rm /usr/bin/kvmtop

# Remove source directory if you built from source
rm -rf ~/kvmtop
```

## Next Steps

- [Usage Guide](usage.md) - Learn how to use kvmtop
- [Configuration](configuration.md) - Customize kvmtop behavior
- [Troubleshooting](troubleshooting.md) - Solve common issues


# kvmtop Documentation

Welcome to the kvmtop documentation! This tool provides real-time monitoring for KVM/QEMU virtual machines on Linux.

## Quick Links

- [Installation Guide](installation.md) - How to install and set up kvmtop
- [Usage Guide](usage.md) - Command-line options and keyboard shortcuts
- [Configuration](configuration.md) - Customizing kvmtop behavior
- [Troubleshooting](troubleshooting.md) - Common issues and solutions
- [Development](development.md) - Contributing and building from source
- [Changelog](changelog.md) - Version history and updates

## Views Documentation

kvmtop provides four specialized monitoring views:

- [Process View](views/process.md) - CPU, memory, and I/O metrics per process/VM
- [Network View](views/network.md) - Network interface statistics and VM mapping
- [Storage View](views/storage.md) - Block device I/O and latency metrics
- [Tree View](views/tree.md) - Hierarchical process and thread visualization

## What is kvmtop?

kvmtop is a specialized, lightweight, real-time monitoring tool designed for Linux servers hosting KVM (Kernel-based Virtual Machine) guests. It bridges the visibility gap by automatically correlating low-level system processes with high-level Virtual Machine identities.

### Key Features

- **Auto-Discovery:** Automatically maps PIDs and network interfaces to VM IDs and names
- **Zero Dependencies:** Runs as a single static binary with no external dependencies
- **Multi-View Dashboard:** Four specialized views for different monitoring needs
- **Latency Focus:** Highlights I/O wait times to identify storage bottlenecks
- **Network Visibility:** Shows network traffic per VM with automatic interface mapping
- **Interactive Controls:** Real-time filtering, customizable refresh rates, and flexible sorting

### System Requirements

- **OS:** Linux (kernel 2.6.26+, any distribution)
- **Architecture:** x86_64 (amd64)
- **Permissions:** Root recommended (required for full I/O statistics)
- **Dependencies:** None! Statically linked binary

## Getting Started

1. [Download or build kvmtop](installation.md)
2. Run with `sudo ./kvmtop`
3. Use keyboard shortcuts to navigate views (see [Usage Guide](usage.md))
4. Press `h` for in-app help

## Support

For issues, feature requests, or contributions, visit the [GitHub repository](https://github.com/yohaya/kvmtop).


# kvmtop

**kvmtop** is a specialized, lightweight, real-time monitoring tool for Linux servers hosting KVM/QEMU virtual machines (Proxmox VE, Libvirt/QEMU).

It bridges the visibility gap by automatically correlating low-level system processes with high-level Virtual Machine identities.

![Process View](https://placeholder-for-screenshot-process-view.png)

## 🚀 Quick Start

```bash
# Download the latest static binary
wget https://github.com/yossibmoha/kvmtop/releases/download/v1.0.2/kvmtop-linux-amd64-static
chmod +x kvmtop-linux-amd64-static
sudo mv kvmtop-linux-amd64-static /usr/local/bin/kvmtop

# Run (sudo recommended for full I/O statistics)
sudo kvmtop
```

**Or build from source:**

```bash
git clone https://github.com/yossibmoha/kvmtop.git
cd kvmtop
make
sudo ./build/kvmtop
```

## ✨ Key Features

- **Auto-Discovery:** Automatically maps PIDs and network interfaces to VM IDs and names
- **Zero Dependencies:** Single static binary - no Python, no libraries, no installation
- **Multi-View Dashboard:** Four specialized views (Process, Tree, Network, Storage)
- **Latency Focus:** Highlights I/O wait times to instantly spot storage bottlenecks
- **Network Visibility:** Dedicated view for VM network traffic with automatic interface mapping
- **Interactive Controls:** Real-time filtering, customizable refresh rates, flexible sorting
- **Help Screen:** Press `h` for in-app keyboard shortcut reference
- **Color Coding:** Visual indicators for critical values (CPU, I/O wait, errors)

## 📖 Documentation

Complete documentation is available in the [`docs/`](docs/) folder:

- **[Installation Guide](docs/installation.md)** - Detailed installation instructions and system requirements
- **[Usage Guide](docs/usage.md)** - Command-line options and all keyboard shortcuts
- **[Configuration](docs/configuration.md)** - Customizing kvmtop with `~/.kvmtoprc`
- **[Troubleshooting](docs/troubleshooting.md)** - Common issues and solutions
- **[Development Guide](docs/development.md)** - Contributing and building from source

### View-Specific Documentation

- **[Process View](docs/views/process.md)** - CPU, memory, and I/O metrics explained
- **[Network View](docs/views/network.md)** - Network interface statistics and VM mapping
- **[Storage View](docs/views/storage.md)** - Block device I/O and latency metrics
- **[Tree View](docs/views/tree.md)** - Hierarchical process and thread visualization

## 🎯 System Requirements

- **OS:** Linux (kernel 2.6.26+, any distribution)
- **Architecture:** x86_64 (amd64)
- **Permissions:** Root recommended (required for full I/O statistics)
- **Dependencies:** None! Statically linked binary

**Tested on:** Debian, Ubuntu, RHEL, CentOS, Rocky Linux, Alpine, Proxmox VE

## ⌨️ Quick Reference

| Key | Function |
|-----|----------|
| `h` | Show help screen |
| `c` | Process/CPU view (default) |
| `s` | Storage/disk view |
| `n` | Network view |
| `t` | Toggle tree mode (threads) |
| `/` | Filter by name/PID/user/VM |
| `f` | Freeze/resume display |
| `l` | Set display limit |
| `r` | Set refresh interval |
| `q` | Quit |

Press `1-8` to sort by different columns (varies by view). Press `h` for complete shortcuts.

## 🔧 Examples

```bash
# Run with 2-second refresh interval
sudo kvmtop --interval 2.0

# Monitor specific process IDs
sudo kvmtop --pid 1234 --pid 5678

# Check version
kvmtop --version
```

## 🎨 What Makes kvmtop Different?

Traditional tools like `top`, `htop`, and `iotop` are excellent for general-purpose monitoring, but they fall short in virtualized environments:

| Challenge | Traditional Tools | kvmtop |
|-----------|-------------------|--------|
| **VM Identification** | Shows only `qemu-system-x86` | Automatically extracts VMID and VM name |
| **Network Mapping** | Lists `tap105i0` without context | Maps tap interfaces directly to VM names |
| **I/O Latency** | Not visible or buried in metrics | Prominently displays `IO_Wait` as key bottleneck indicator |
| **Thread Analysis** | Flat list of TIDs | Tree view showing main process + worker threads |
| **Storage Focus** | Generic disk stats | Dedicated storage view with per-device latency |
| **Zero Setup** | Usually requires packages | Single static binary, no installation |

kvmtop bridges the gap between low-level system metrics and high-level virtualization context.

## 📊 Use Cases

- **Performance Troubleshooting:** Identify CPU, I/O, or network bottlenecks per VM
- **Capacity Planning:** Monitor resource utilization and plan for growth
- **Health Checks:** Quick overview of VM and host system health
- **Proxmox VE Administration:** Perfect complement to Proxmox web interface

## 🤝 Contributing

Contributions are welcome! See the [Development Guide](docs/development.md) for:

- Building from source
- Code style guidelines
- Testing procedures
- Submitting pull requests

For bugs or feature requests, visit [GitHub Issues](https://github.com/yossibmoha/kvmtop/issues).

## 📜 License

kvmtop is licensed under the **GNU General Public License v3.0**. See [LICENSE](LICENSE) for details.

## 🌟 Support

If you find kvmtop useful, please:

- ⭐ Star this project on [GitHub](https://github.com/yossibmoha/kvmtop)
- 🐛 Report issues or suggest features
- 📖 Improve documentation
- 💻 Contribute code

---

**Made with ❤️ for the KVM/Proxmox community**

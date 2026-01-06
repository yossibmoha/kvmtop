# kvmtop

**kvmtop** is a specialized, lightweight, real-time monitoring tool designed for Linux servers hosting KVM (Kernel-based Virtual Machine) guests (e.g., Proxmox VE, plain Libvirt/QEMU). 

It bridges the visibility gap by automatically correlating low-level system processes with high-level Virtual Machine identities.

![Process View](https://placeholder-for-screenshot-process-view.png)

## 🚀 Key Features

1.  **Auto-Discovery:** Parsing `/proc` to instantly map PIDs and TAP interfaces to VMIDs and Names.
2.  **Zero Dependencies:** Running as a single, static binary. No Python, no Libvirt libraries, no installation.
3.  **Multi-View Dashboard:** Four specialized views - Process, Tree, Network, and Storage monitoring.
4.  **Latency Focus:** Highlighting `IO_Wait` to instantly spot storage bottlenecks affecting specific VMs.
5.  **Network Visibility:** Dedicated view for VM network traffic, auto-associated with the correct VM.
6.  **Interactive Controls:** Real-time filtering, customizable refresh rates, and flexible sorting options.

## 📦 Installation

### System Requirements
- **OS:** Linux (kernel 2.6.26+, any distribution)
- **Architecture:** x86_64 (amd64)
- **Permissions:** Root recommended (required for full I/O statistics)
- **Dependencies:** None! Statically linked binary.

### Quick Start (Pre-compiled Binary)
The easiest way to run `kvmtop` is to download the latest static binary. It works on **any** x86_64 Linux distribution (Debian, Ubuntu, CentOS, RHEL, Alpine, Proxmox).

```bash
# Download and run in one step
curl -L -o kvmtop https://github.com/yohaya/kvmtop/releases/download/latest/kvmtop-static-linux-amd64
chmod +x kvmtop
sudo ./kvmtop
```

*(Note: `sudo` is highly recommended to view Disk I/O statistics, which are restricted by the kernel to root users.)*

### Building from Source
If you prefer to build it yourself:

1.  **Prerequisites:** `gcc`, `make`.
2.  **Build:**
    ```bash
    git clone https://github.com/yohaya/kvmtop.git
    cd kvmtop
    make
    sudo ./build/kvmtop
    ```

### Command-Line Options
```bash
kvmtop [OPTIONS]

Options:
  -i, --interval <seconds>   Set refresh interval (default: 5.0)
  -p, --pid <PID>            Monitor specific process ID(s) (can be repeated)
  -h, --help                 Show help message

Examples:
  kvmtop                     # Run with default settings
  kvmtop -i 2.0              # Update every 2 seconds
  kvmtop -p 1234 -p 5678     # Monitor only PID 1234 and 5678
```

## 🖥️ Usage & Interface

### 1. Process View (Default - Press `c`)
This is the main dashboard. It focuses on CPU and Disk performance per process/VM.

| Column | Metric | Detailed Explanation |
| :--- | :--- | :--- |
| **PID** | Process ID | The OS Process ID. For KVM, this is the main QEMU process. |
| **User** | Owner | The user account running the process. |
| **Uptime** | Runtime | How long the process has been running (format: days/hours or HH:MM:SS). |
| **Res(MiB)** | Resident Memory | Physical RAM currently used by the process. |
| **Shr(MiB)** | Shared Memory | Memory shared with other processes. |
| **Virt(MiB)** | Virtual Memory | Total virtual memory allocated. |
| **R_Log / W_Log** | **Logical** Read/Write IOPS | The number of `read()` or `write()` system calls the application made per second. *High R_Log + Low R_MiB/s = Excellent RAM Caching.* |
| **Wait** | **Latency** (ms) | Total milliseconds the process spent **blocked** waiting for physical Disk I/O. **This is your primary bottleneck indicator.** High values (>1000ms) mean the storage array is too slow for the VM. |
| **R_MiB/s** | Read Bandwidth | Physical data read from the storage layer (excluding page cache hits). |
| **W_MiB/s** | Write Bandwidth | Physical data written to the storage layer. |
| **CPU%** | CPU Usage | Real-time CPU usage. Can exceed 100% for multi-threaded processes (1 core = 100%). |
| **S** | State | Process state (R=Running, S=Sleeping, D=Disk Wait, Z=Zombie). |
| **COMMAND** | Context | Shows the VMID and Name (e.g., `100 - database-vm`) if identified, or the process name. |

---

### 2. Network View (Press `n`)
Switches to a view focusing on network interfaces, specifically tuned for virtualization.

*   **Auto-Filtering:** Automatically hides host firewall interfaces (`fw*`) and loopback to reduce noise.
*   **Top 50:** Shows the top 50 busiest interfaces sorted by Transmit rate (default).
*   **VM Mapping:** Matches `tap` interfaces (e.g., `tap105i0`) to their owning VM (e.g., `105 - webserver`).

| Column | Explanation |
| :--- | :--- |
| **IFACE** | Interface Name (e.g., `eth0`, `tap100i0`). |
| **STATE** | Link state (UP/DOWN/unknown). |
| **RX_Mbps** | Receive rate in Megabits per second. |
| **TX_Mbps** | Transmit rate in Megabits per second. |
| **RX_Pkts / TX_Pkts** | Packets per second (PPS). High PPS with low Mbps indicates small packet storm (DDoS/DNS). |
| **RX_Err / TX_Err** | Error packets per second. |
| **VMID / VM_NAME** | The ID and Name of the VM this interface belongs to. |

---

### 3. Storage View (Press `s`) 🆕
Displays block device I/O statistics from `/proc/diskstats`, showing physical storage performance.

*   **Device-Level Metrics:** Shows all physical and virtual block devices (excludes loop and ram devices).
*   **Latency Tracking:** Average read/write latency per operation in milliseconds.
*   **Sortable:** Sort by IOPS, bandwidth, or latency to identify bottlenecks.

| Column | Explanation |
| :--- | :--- |
| **DEVICE** | Block device name (e.g., `sda`, `nvme0n1`, `vda`). |
| **R_IOPS / W_IOPS** | Read/Write operations per second at the device level. |
| **R_MiB/s / W_MiB/s** | Physical read/write throughput to the device. |
| **R_Lat(ms) / W_Lat(ms)** | Average latency per read/write operation. High values indicate storage bottlenecks. |

---

### 4. Tree View (Press `t`)
Toggles a tree visualization in the Process View.
*   Shows the main QEMU process with aggregated metrics.
*   Lists all worker threads (vCPUs, IO threads) indented beneath it with individual statistics.
*   Useful for identifying if a single vCPU is pegged at 100% inside a large VM.

## ⌨️ Keyboard Shortcuts

### View Controls
| Key | Function |
| :--- | :--- |
| **`c`** | **Process/CPU Mode:** Switch to the main process view. |
| **`s`** | **Storage Mode:** Switch to the storage/disk view. |
| **`n`** | **Network Mode:** Switch to the network interface view. |
| **`t`** | **Tree Mode:** Toggle thread tree visualization (in Process mode). |

### Interactive Controls
| Key | Function |
| :--- | :--- |
| **`f`** | **Freeze:** Pause/Resume display updates (useful to copy text). |
| **`l`** | **Limit:** Interactively set the number of processes to display (default: 50). |
| **`r`** | **Refresh:** Interactively set the refresh interval in seconds (default: 5.0). |
| **`/`** | **Filter:** Enter filter mode to search/filter by PID, command, user, or VM name. Press ESC to cancel, Enter to apply. |

### Sorting (Context-Dependent)
**Process Mode:**
| Key | Function |
| :--- | :--- |
| **`1`** | Sort by PID (toggle ascending/descending). |
| **`2`** | Sort by CPU% (toggle ascending/descending). |
| **`3`** | Sort by Read Logs (logical IOPS). |
| **`4`** | Sort by Write Logs (logical IOPS). |
| **`5`** | Sort by IO Wait (latency). |
| **`6`** | Sort by Read Bandwidth (MiB/s). |
| **`7`** | Sort by Write Bandwidth (MiB/s). |
| **`8`** | Sort by State. |

**Network Mode:**
| Key | Function |
| :--- | :--- |
| **`1`** | Sort by RX (Receive Mbps). |
| **`2`** | Sort by TX (Transmit Mbps). |

**Storage Mode:**
| Key | Function |
| :--- | :--- |
| **`1`** | Sort by Read IOPS. |
| **`2`** | Sort by Write IOPS. |
| **`3`** | Sort by Read MiB/s. |
| **`4`** | Sort by Write MiB/s. |
| **`5`** | Sort by Read Latency. |
| **`6`** | Sort by Write Latency. |

### General
| Key | Function |
| :--- | :--- |
| **`q`** | **Quit** the application. |

## 🔧 Advanced Features

### Real-Time Filtering
Press **`/`** to enter filter mode. Type any text to filter processes, network interfaces, or disk devices by:
- Process name or command
- PID
- User name
- VM ID or VM name
- Interface name (in network mode)
- Device name (in storage mode)

Press **Enter** to apply, **ESC** to cancel. The filter persists until you clear it or change it.

### Customizable Display
- **Limit (`l`)**: Control how many entries are shown (default: 50). Useful for focusing on top consumers or viewing more data on large screens.
- **Refresh Rate (`r`)**: Adjust the sampling interval (default: 5.0 seconds). Lower values (e.g., 1.0s) provide more responsive monitoring but higher CPU overhead.

### Multi-Threaded Process Analysis
Enable Tree View (`t`) to see individual thread-level statistics for multi-threaded applications like QEMU/KVM. This helps identify:
- Which vCPU is consuming the most host CPU
- I/O threads causing latency
- Helper threads (VNC, migration, etc.)

## 🛠️ Troubleshooting

**Q: Why do I see 0.00 for R_MiB/s and IO_Wait?**  
**A:** You are likely running as a non-root user. The Linux kernel restricts access to `/proc/[pid]/io` (where these stats live) to the root user for security. Please run with `sudo`.

**Q: I don't see VM names, just command lines.**  
**A:** `kvmtop` parses standard QEMU/KVM command lines (Proxmox style with `-id` and `-name` flags). If you are using a custom wrapper or different libvirt naming convention, the parser might miss the name. Feel free to open an issue with your process command line structure.

**Q: The display is flickering or garbled.**  
**A:** Ensure your terminal supports ANSI escape codes. Most modern terminals (xterm, gnome-terminal, iTerm2, etc.) work fine. If running over SSH, make sure your `TERM` variable is set correctly (e.g., `TERM=xterm-256color`).

**Q: Can I monitor specific VMs only?**  
**A:** Yes! Use the `--pid` flag to filter by process ID, or use the interactive filter (`/`) to search by VM name or ID during runtime.

**Q: High CPU usage from kvmtop itself?**  
**A:** `kvmtop` scans `/proc` every refresh interval. On systems with thousands of processes or very short refresh intervals (<1s), this can be noticeable. Try increasing the refresh rate with `r` or using `--interval` flag.

## 🎯 Why kvmtop?

Traditional monitoring tools like `top`, `htop`, and `iotop` are excellent for general-purpose monitoring, but they fall short in virtualized environments:

| Challenge | Traditional Tools | kvmtop |
| :--- | :--- | :--- |
| **VM Identification** | Shows only `qemu-system-x86` | Automatically extracts VMID and VM name from command line |
| **Network Mapping** | Lists `tap105i0` without context | Maps tap interfaces directly to VM names |
| **I/O Latency** | Not visible or buried in metrics | Prominently displays `IO_Wait` as a key bottleneck indicator |
| **Thread Analysis** | Flat list of TIDs | Tree view showing main process + worker threads |
| **Storage Focus** | Generic disk stats | Dedicated storage view with per-device latency |
| **Zero Setup** | Usually requires packages/Python | Single static binary, no installation |

**kvmtop bridges the gap between low-level system metrics and high-level virtualization context**, making it ideal for:
- Proxmox VE administrators
- QEMU/KVM operators
- Performance troubleshooting
- Capacity planning
- Quick health checks

## 🤝 Contributing

Contributions are welcome! If you encounter issues or have feature requests:
1. Check existing issues at [GitHub Issues](https://github.com/yohaya/kvmtop/issues)
2. For bugs, include: OS version, kvmtop version, and reproduction steps
3. For features, describe the use case and expected behavior

Pull requests should:
- Follow the existing C coding style
- Include clear commit messages
- Test on at least one KVM environment

## 📜 License

This project is licensed under the **GNU General Public License v3.0**.  
See [LICENSE](LICENSE) file for full text.

---

**Made with ❤️ for the KVM/Proxmox community**  
⭐ Star this project on [GitHub](https://github.com/yohaya/kvmtop) if you find it useful!

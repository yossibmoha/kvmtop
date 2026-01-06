# Usage Guide

Complete guide to using kvmtop, including command-line options and all keyboard shortcuts.

## Command-Line Options

```bash
kvmtop [OPTIONS]
```

### Options

| Option | Long Form | Argument | Description |
|--------|-----------|----------|-------------|
| `-i` | `--interval` | `<seconds>` | Set refresh interval (default: 5.0) |
| `-p` | `--pid` | `<PID>` | Monitor specific process ID(s), can be repeated |
| `-v` | `--version` | - | Show version information and exit |
| `-h` | `--help` | - | Show help message and exit |

### Examples

```bash
# Run with default settings (5-second refresh)
sudo kvmtop

# Update every 2 seconds
sudo kvmtop --interval 2.0

# Monitor only specific PIDs
sudo kvmtop --pid 1234 --pid 5678

# Short form
sudo kvmtop -i 1.0 -p 9999

# Check version
kvmtop --version
```

## Keyboard Shortcuts

Press `h` at any time to view the in-app help screen.

### View Controls

Switch between different monitoring views:

| Key | View | Description |
|-----|------|-------------|
| `c` | **Process/CPU View** | Main dashboard showing CPU, memory, and I/O metrics |
| `s` | **Storage View** | Block device I/O statistics and latency |
| `n` | **Network View** | Network interface traffic and VM mapping |
| `t` | **Tree View** | Toggle thread tree visualization (in Process mode) |
| `h` | **Help Screen** | Show keyboard shortcut reference |
| `e` | **Export** | Export current view to CSV file |

### Interactive Controls

| Key | Function | Description |
|-----|----------|-------------|
| `f` | **Freeze/Resume** | Pause or resume display updates (useful for reading) |
| `l` | **Limit** | Set number of entries to display (default: 50) |
| `r` | **Refresh** | Set refresh interval in seconds (default: 5.0) |
| `/` | **Filter** | Enter filter mode to search by PID, name, user, or VM |
| `q` | **Quit** | Exit kvmtop |

### Sorting (htop-style)

kvmtop supports htop-style sorting using **function keys (F1-F8)** or **number keys (1-8)**:

> **Tip:** Press the same key twice to toggle between ascending and descending order. The sort indicator (`v` or `^`) shows current direction.

### Sorting - Process View

| Key | Alt Key | Sort By | Description |
|-----|---------|---------|-------------|
| `F1` | `1` | PID | Process ID |
| `F2` | `2` | CPU% | CPU usage percentage |
| `F3` | `3` | R_Log | Read logical IOPS (system calls) |
| `F4` | `4` | W_Log | Write logical IOPS (system calls) |
| `F5` | `5` | Wait | I/O wait time in milliseconds |
| `F6` | `6` | R_MiB | Read bandwidth in MiB/s |
| `F7` | `7` | W_MiB | Write bandwidth in MiB/s |
| `F8` | `8` | State | Process state (R/S/D/Z) |

### Sorting - Network View

| Key | Alt Key | Sort By | Description |
|-----|---------|---------|-------------|
| `F1` | `1` | RX | Receive rate in Mbps |
| `F2` | `2` | TX | Transmit rate in Mbps |

### Sorting - Storage View

| Key | Alt Key | Sort By | Description |
|-----|---------|---------|-------------|
| `F1` | `1` | R_IOPS | Read operations per second |
| `F2` | `2` | W_IOPS | Write operations per second |
| `F3` | `3` | R_MiB/s | Read throughput |
| `F4` | `4` | W_MiB/s | Write throughput |
| `F5` | `5` | R_Lat | Read latency in milliseconds |
| `F6` | `6` | W_Lat | Write latency in milliseconds |

## Interactive Features

### Filtering

Press `/` to enter filter mode. Type any text to filter entries by:

- Process name or command
- PID (Process ID)
- User name
- VM ID or VM name (in Process view)
- Interface name (in Network view)
- Device name (in Storage view)

**Controls in filter mode:**
- Type to add characters
- `Backspace` to delete characters
- `Enter` to apply filter
- `ESC` to cancel without filtering

**Example workflow:**
1. Press `/`
2. Type `nginx`
3. Press `Enter`
4. Only processes/entries matching "nginx" will be shown
5. Press `/` again and clear to remove filter

### Setting Display Limit

Press `l` to change how many entries are displayed (default: 50).

**Controls in limit mode:**
- Type a number (e.g., `100`)
- `Backspace` to delete digits
- `Enter` to apply
- `ESC` to cancel

### Adjusting Refresh Interval

Press `r` to change the refresh interval (default: 5.0 seconds).

**Controls in refresh mode:**
- Type a number with optional decimal (e.g., `2.5`)
- `Backspace` to delete characters
- `Enter` to apply
- `ESC` to cancel

**Recommendations:**
- **1-2 seconds:** High-frequency monitoring, higher CPU overhead
- **5 seconds:** Good balance (default)
- **10+ seconds:** Lower overhead, less responsive

### Freezing the Display

Press `f` to freeze/resume the display. Useful for:
- Reading detailed information
- Copying text from the terminal
- Taking screenshots
- Analyzing a specific moment in time

While frozen:
- The display won't update
- You can still use keyboard shortcuts
- Press `f` again to resume updates

## Understanding the Output

See the view-specific documentation for detailed column explanations:

- [Process View](views/process.md) - Detailed column descriptions
- [Network View](views/network.md) - Network metrics explained
- [Storage View](views/storage.md) - Disk I/O metrics
- [Tree View](views/tree.md) - Thread hierarchy

## Tips and Best Practices

### Performance Monitoring

1. **Start with Process View (`c`)** to get an overview of CPU and I/O
2. **Look for high IO Wait** values (red = critical bottleneck)
3. **Switch to Storage View (`s`)** to identify which disk is slow
4. **Check Network View (`n`)** for network-bound VMs

### Finding Specific VMs

```bash
# Method 1: Use filter (/)
1. Press /
2. Type VM name or ID
3. Press Enter

# Method 2: Use --pid flag
sudo kvmtop --pid $(pgrep -f "vmid-100")
```

### Monitoring During Issues

```bash
# Fast refresh for troubleshooting
sudo kvmtop --interval 1.0

# Focus on top CPU consumers
1. Press c (Process view)
2. Press 2 (Sort by CPU)
3. Press l, type 10, Enter (Show top 10)
```

### Long-Term Monitoring

```bash
# Run in tmux/screen for persistent monitoring
tmux new -s kvmtop
sudo kvmtop

# Detach: Ctrl+B, then D
# Reattach: tmux attach -t kvmtop
```

## Troubleshooting

For common issues and solutions, see the [Troubleshooting Guide](troubleshooting.md).

## Next Steps

- [Views Documentation](views/process.md) - Understand what each metric means
- [Configuration](configuration.md) - Customize default behavior
- [Troubleshooting](troubleshooting.md) - Solve common problems


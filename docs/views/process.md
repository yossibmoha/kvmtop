# Process View Documentation

The Process View is the main dashboard of kvmtop, showing CPU, memory, and I/O metrics for each process or virtual machine.

## Access

- **Keyboard:** Press `c` to switch to Process View
- **Default:** Process View is the default view on startup

## Overview

The Process View displays real-time metrics for all processes (or filtered subset), with a focus on KVM/QEMU virtual machines. Each row represents one process (TGID), with all threads aggregated together.

## Column Reference

### Process Identification

| Column | Full Name | Description |
|--------|-----------|-------------|
| **PID** | Process ID | The OS Process ID (TGID). For KVM, this is the main QEMU process. Click `1` to sort by PID. |
| **User** | Owner | The user account running the process (e.g., `root`, `libvirt-qemu`). |
| **COMMAND** | Command/VM Name | Shows VM ID and Name (e.g., `100 - database-vm`) if identified, otherwise the process name. |

### Memory Metrics

| Column | Full Name | Unit | Description |
|--------|-----------|------|-------------|
| **Res(MiB)** | Resident Memory | MiB | Physical RAM currently used by the process (Resident Set Size). This is "real" memory usage. |
| **Shr(MiB)** | Shared Memory | MiB | Memory shared with other processes. Libraries loaded by multiple processes count here. |
| **Virt(MiB)** | Virtual Memory | MiB | Total virtual memory allocated (may be much larger than physical RAM). Includes swap and unmapped pages. |

**Understanding Memory:**
- `Res` is the most important - actual RAM used
- `Virt` is often very large for VMs (shows allocated VM RAM)
- `Res < Virt` is normal and expected

### I/O Metrics - Logical Operations

| Column | Full Name | Unit | Description |
|--------|-----------|------|-------------|
| **R_Log** | Logical Read IOPS | ops/sec | Number of `read()` system calls per second. High value = application reading frequently. |
| **W_Log** | Logical Write IOPS | ops/sec | Number of `write()` system calls per second. High value = application writing frequently. |

**Key Insight:** High logical IOPS with low physical bandwidth = excellent page cache performance!

### I/O Metrics - Physical Operations

| Column | Full Name | Unit | Description |
|--------|-----------|------|-------------|
| **Wait** | I/O Wait Time | ms | **CRITICAL METRIC** - Total milliseconds the process spent blocked waiting for disk I/O during the interval. High values (>1000ms) indicate storage bottlenecks. |
| **R_MiB/s** | Physical Read Bandwidth | MiB/sec | Physical data read from storage (excludes page cache hits). Shows actual disk read rate. |
| **W_MiB/s** | Physical Write Bandwidth | MiB/sec | Physical data written to storage. Shows actual disk write rate. |

**I/O Wait Interpretation:**
- 🟢 **< 500ms:** Normal, storage keeping up
- 🟡 **500-1000ms:** Moderate delay, monitor
- 🔴 **> 1000ms:** Severe bottleneck, investigate storage

### CPU Metrics

| Column | Full Name | Unit | Description |
|--------|-----------|------|-------------|
| **CPU%** | CPU Usage | percent | Real-time CPU usage. Can exceed 100% for multi-threaded processes (1 core = 100%, so 4 cores fully used = 400%). Click `2` to sort. |

**CPU% Notes:**
- Single-threaded process: max 100%
- Multi-threaded: max = (num_cores × 100%)
- 🟢 < 80%, 🟡 80-95%, 🔴 > 95% (per process)

### Process State

| Column | Full Name | Description |
|--------|-----------|-------------|
| **S** | State | Process state: **R** (Running), **S** (Sleeping/Idle), **D** (Uninterruptible Disk Wait - 🔴), **Z** (Zombie - 🟡). |

**State Meanings:**
- `R`: Currently executing on CPU
- `S`: Waiting for event (normal idle state)
- `D`: Blocked on disk I/O (indicates I/O bottleneck)
- `Z`: Zombie process (parent hasn't reaped it)

### Uptime

| Column | Full Name | Format | Description |
|--------|-----------|--------|-------------|
| **Uptime** | Process Runtime | `dd:hh` or `HH:MM:SS` | How long the process has been running. Format: `XdYYh` for days/hours, or `HH:MM:SS` for sub-day. |

## Sorting Options

Press number keys to sort by different columns:

| Key | Column | Description |
|-----|--------|-------------|
| `1` | PID | Sort by Process ID |
| `2` | CPU% | Sort by CPU usage (default) |
| `3` | R_Log | Sort by logical read IOPS |
| `4` | W_Log | Sort by logical write IOPS |
| `5` | Wait | Sort by I/O wait time (find bottlenecks!) |
| `6` | R_MiB | Sort by physical read bandwidth |
| `7` | W_MiB | Sort by physical write bandwidth |
| `8` | State | Sort by process state |

**Tip:** Press the same key twice to toggle ascending/descending order.

## Interpreting the Data

### Finding CPU-Bound VMs

1. Sort by CPU% (press `2`)
2. Look for processes consistently near 100% per core
3. Consider:
   - Is the VM allocated enough vCPUs?
   - Is there CPU contention (too many VMs on host)?

### Finding I/O-Bound VMs

1. Sort by Wait (press `5`)
2. Look for high Wait values (> 1000ms)
3. Cross-reference with Storage View (`s` key):
   - Which disk is the VM using?
   - Is that disk saturated?

### Cache Hit Analysis

**Good caching:**
- High R_Log (e.g., 5000 ops/s)
- Low R_MiB/s (e.g., 10 MiB/s)
- Low Wait (e.g., < 100ms)

**Interpretation:** Application is doing many small reads, but most hit page cache.

**Poor caching:**
- High R_Log (e.g., 5000 ops/s)
- High R_MiB/s (e.g., 500 MiB/s)
- High Wait (e.g., 2000ms)

**Interpretation:** Application doing many reads that miss cache and hit disk.

### Memory Issues

**Overcommit detection:**
- Check if `Sum(Res)` > Physical RAM
- If yes, system is overcommitted (risky!)

**Swapping detection:**
- High I/O Wait + High R/W bandwidth + Low logical IOPS = possible swap activity
- Confirm with `free -h` or `vmstat`

## Aggregated Summary

At the bottom of the view, kvmtop shows totals:

```
TOTAL      12,345  8,192  524,288  12,500  8,300  125.4  ...
```

- **Res/Shr/Virt:** Sum of all process memory
- **R_Log/W_Log:** Total system logical I/O rate
- **Wait:** Sum of all processes' I/O wait
- **R_MiB/W_MiB:** Total system physical I/O bandwidth
- **CPU%:** Sum of all processes' CPU usage

**Use totals to:**
- Check overall system utilization
- Identify if load is distributed or concentrated
- Compare against hardware limits

## Tree View Mode

Press `t` to toggle Tree View, which shows:

1. **Main process** (TGID) with aggregated metrics
2. **Individual threads** (TIDs) indented beneath with per-thread stats

### When to Use Tree View

- Debugging multi-threaded applications
- Identifying which vCPU thread is consuming CPU
- Finding I/O threads causing latency
- Understanding thread distribution

### Example

```
1234  user  01:23:45  8192  ... 150.5  S  100 - web-vm
  └─ 1235  user  01:23:45  2048  ...  50.0  R  [vCPU 0]
  └─ 1236  user  01:23:45  2048  ... 100.0  R  [vCPU 1]
  └─ 1237  user  01:23:45  2048  ...   0.5  S  [IO Thread]
```

## Tips and Tricks

### Find Most Active VM

```
1. Press '2' to sort by CPU
2. Look at top entry
```

### Find Storage Bottleneck

```
1. Press '5' to sort by Wait
2. Top entry shows which VM is blocked on I/O
3. Press 's' to see which disk is slow
```

### Monitor Specific VM

```
# If you know the PID
kvmtop --pid 1234

# Or use filter interactively
1. Press '/'
2. Type VM name or ID
3. Press Enter
```

### Compare Before/After

```
1. Press 'f' to freeze
2. Take screenshot or note values
3. Make system change
4. Press 'f' to resume
5. Compare new values
```

## Color Coding

When colors are enabled (default):

- 🟢 **Green CPU:** < 80% (healthy)
- 🟡 **Yellow CPU:** 80-95% (busy)
- 🔴 **Red CPU:** > 95% (saturated)

- **Green Wait:** < 500ms (fast storage)
- 🟡 **Yellow Wait:** 500-1000ms (moderate delay)
- 🔴 **Red Wait:** > 1000ms (slow storage)

- 🔴 **Red State 'D':** Process blocked on disk (bottleneck indicator)

## Next Steps

- [Storage View](storage.md) - Drill down into disk bottlenecks
- [Network View](network.md) - Check network bandwidth
- [Tree View](tree.md) - Analyze thread-level behavior
- [Usage Guide](../usage.md) - Learn all keyboard shortcuts


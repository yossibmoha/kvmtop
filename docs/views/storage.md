# Storage View Documentation

The Storage View displays block device I/O statistics and latency metrics, essential for identifying storage bottlenecks in virtualized environments.

## Access

- **Keyboard:** Press `s` to switch to Storage View
- **From other views:** Press `s` at any time

## Overview

Storage View shows real-time metrics from `/proc/diskstats` for all physical and virtual block devices. It helps identify:

- Which disks are bottlenecks
- Read vs write performance
- Latency per operation
- Overall disk utilization

**Filtered out automatically:** loop devices, ram disks

## Column Reference

| Column | Full Name | Unit | Description |
|--------|-----------|------|-------------|
| **DEVICE** | Device Name | - | Block device name (e.g., `sda`, `nvme0n1`, `vda`, `sdb1`). |
| **R_IOPS** | Read Operations/sec | ops/s | Number of read operations completed per second at the device level. Click `1` to sort. |
| **W_IOPS** | Write Operations/sec | ops/s | Number of write operations completed per second at the device level. Click `2` to sort. |
| **R_MiB/s** | Read Throughput | MiB/s | Physical data read from the device per second. Click `3` to sort. |
| **W_MiB/s** | Write Throughput | MiB/s | Physical data written to the device per second. Click `4` to sort. |
| **R_Lat(ms)** | Read Latency | ms | Average time per read operation. Low is better. Click `5` to sort. |
| **W_Lat(ms)** | Write Latency | ms | Average time per write operation. Low is better. Click `6` to sort. |

## Sorting Options

| Key | Sort By | Description |
|-----|---------|-------------|
| `1` | R_IOPS | Sort by read operations per second (default) |
| `2` | W_IOPS | Sort by write operations per second |
| `3` | R_MiB/s | Sort by read throughput |
| `4` | W_MiB/s | Sort by write throughput |
| `5` | R_Lat | Sort by read latency (higher = slower) |
| `6` | W_Lat | Sort by write latency (higher = slower) |

**Tip:** Press the same key twice to toggle between ascending/descending order.

## Understanding the Metrics

### IOPS (Operations Per Second)

**What it measures:** Number of I/O operations completed, regardless of size.

**Typical ranges:**

| Storage Type | Sequential IOPS | Random IOPS |
|--------------|-----------------|-------------|
| HDD (7200 RPM) | 100-200 | 75-100 |
| HDD (15K RPM) | 200-400 | 150-200 |
| SATA SSD | 30K-90K | 20K-80K |
| NVMe SSD | 200K-1M+ | 100K-500K+ |
| RAM Disk | Millions | Millions |

**Interpretation:**
- **Low IOPS on SSD:** Likely sequential I/O (large reads/writes)
- **High IOPS on HDD:** Possible bottleneck (HDDs struggle with random I/O)
- **Very high IOPS:** Either very fast storage or small I/O operations

### Throughput (MiB/s)

**What it measures:** Amount of data transferred.

**Calculation:** `Throughput = IOPS × Average I/O Size`

**Typical maximum throughput:**

| Storage Type | Max Sequential Read | Max Sequential Write |
|--------------|---------------------|----------------------|
| HDD (7200 RPM) | 100-150 MiB/s | 100-150 MiB/s |
| SATA SSD | 500-550 MiB/s | 400-500 MiB/s |
| NVMe Gen3 | 3,000-3,500 MiB/s | 2,000-3,000 MiB/s |
| NVMe Gen4 | 5,000-7,000 MiB/s | 4,000-6,000 MiB/s |

**Interpretation:**
- **Near max:** Device is saturated
- **Low despite high IOPS:** Small I/O operations
- **High with low IOPS:** Large I/O operations (sequential)

### Latency (ms)

**What it measures:** Average time from request to completion.

**Critical metric for responsiveness!**

**Typical latencies:**

| Storage Type | Read Latency | Write Latency |
|--------------|--------------|---------------|
| HDD | 5-15 ms | 5-15 ms |
| SATA SSD | 0.1-0.5 ms | 0.1-1.0 ms |
| NVMe SSD | 0.02-0.1 ms | 0.02-0.15 ms |
| RAM Disk | <0.01 ms | <0.01 ms |
| Network Storage (NFS) | 1-10 ms | 1-20 ms |
| Network Storage (iSCSI) | 1-5 ms | 2-10 ms |

**Color coding (when enabled):**
- 🟢 Green: < 10ms (good)
- 🟡 Yellow: 10-50ms (moderate)
- 🔴 Red: > 50ms (slow)

**Troubleshooting high latency:**

| Latency Range | Likely Cause | Action |
|---------------|--------------|--------|
| 0-1 ms | Normal SSD performance | ✅ OK |
| 1-10 ms | Normal HDD or network storage | ✅ OK for HDD |
| 10-50 ms | Busy disk, queue depth issues | Investigate load |
| 50-100 ms | Overloaded storage | Add disks, upgrade storage |
| > 100 ms | Severe bottleneck | **Urgent:** Storage failing or heavily overloaded |

## Device Types

### Physical Disks

- `sda`, `sdb`, `sdc`: SCSI/SATA disks (traditional naming)
- `nvme0n1`, `nvme1n1`: NVMe SSDs
- `hda`, `hdb`: IDE disks (older systems)

**Characteristics:**
- These are real physical devices
- Performance reflects actual hardware capabilities
- Bottlenecks here affect all VMs using this disk

### Virtual Disks

- `vda`, `vdb`: Virtio block devices (VM guest view)
- `xvda`, `xvdb`: Xen virtual disks

**Note:** If you see `vda` in kvmtop, you're running kvmtop **inside** a VM, not on the host!

### Partitions

- `sda1`, `sda2`: Partitions on `sda`
- `nvme0n1p1`: Partition on NVMe device

**Interpretation:**
- Multiple active partitions on same disk compete for I/O
- Sum partition IOPS ≤ Physical disk IOPS

### RAID/LVM

- `md0`, `md1`: Linux software RAID
- `dm-0`, `dm-1`: Device mapper (LVM, dm-crypt)

**Note:** RAID aggregates multiple disks; IOPS may be higher than single disk.

## Identifying Bottlenecks

### Pattern: High Latency

```
DEVICE    R_IOPS  W_IOPS  R_MiB/s  W_MiB/s  R_Lat(ms)  W_Lat(ms)
sda          250     180      12.5      8.2       45.2       62.8
```

**Analysis:**
- Latency > 40ms on HDD → disk is saturated
- Solution: 
  - Reduce load (move VMs to other disks)
  - Upgrade to SSD
  - Add caching layer

### Pattern: High IOPS, Low Throughput

```
DEVICE    R_IOPS  W_IOPS  R_MiB/s  W_MiB/s  R_Lat(ms)  W_Lat(ms)
sda       15,000   8,000      125       85        8.2        12.5
```

**Analysis:**
- High IOPS but moderate throughput → small random I/O
- Average I/O size: 125 MiB / 15,000 = 8.5 KB per operation
- This is hard for HDDs!
- Solution:
  - Migrate to SSD for better random I/O performance
  - Tune application to use larger I/O sizes
  - Add read cache

### Pattern: Write-Heavy Workload

```
DEVICE    R_IOPS  W_IOPS  R_MiB/s  W_MiB/s  R_Lat(ms)  W_Lat(ms)
sda           50   5,000        5      450        2.1       18.5
```

**Analysis:**
- W_IOPS >> R_IOPS → write-heavy application
- High write latency (18.5ms) → disk struggling
- Solution:
  - Add write cache (BBU/supercap)
  - Use RAID with write-back cache
  - SSD with good write performance

### Pattern: Near Line Rate

```
DEVICE    R_IOPS  W_IOPS  R_MiB/s  W_MiB/s  R_Lat(ms)  W_Lat(ms)
nvme0n1   45,000  28,000    3,200    2,850        0.45        0.82
```

**Analysis:**
- NVMe Gen3: Max ~3,500 MiB/s read
- Currently at 3,200 MiB/s (91% utilization)
- **Device is near capacity**
- Solution:
  - Distribute load across multiple NVMe devices
  - Upgrade to Gen4 NVMe (7,000 MiB/s)
  - Add more disks

## Correlating with Process View

### Workflow: Find Which VM is Stressing Storage

1. **Storage View (`s`)**: Identify slow disk
   ```
   sda: High latency (50ms)
   ```

2. **Process View (`c`)**: Sort by I/O wait (`5`)
   ```
   VM 105 (database-vm): IO_Wait = 2,500ms
   ```

3. **Cross-reference:**
   - VM 105 is using `sda`
   - `sda` is slow (50ms latency)
   - **Root cause:** VM 105's database workload is overwhelming `sda`

4. **Solution:**
   - Move VM 105 to faster disk (SSD)
   - Optimize database queries
   - Add read replicas to distribute load

## Use Cases

### Health Check

```
1. Press 's' for Storage View
2. Press '5' to sort by read latency
3. Press '6' to sort by write latency
4. Check: Are any disks > 20ms average?
   - If yes: investigate load
   - If no: storage is healthy
```

### Capacity Planning

```
1. Press 's'
2. Note current IOPS and throughput
3. Compare against device specs
4. Calculate headroom:
   - IOPS utilization = Current / Max
   - If > 70%, consider adding capacity
```

### Troubleshooting Slow VM

```
1. Identify slow VM in Process View (high IO_Wait)
2. Press 's' for Storage View
3. Press '5' or '6' to sort by latency
4. Identify which disk the VM uses
5. Determine if disk is bottleneck
```

## Advanced Metrics (Future)

Future versions may include:

- **%util:** Percentage of time device was busy
- **avgqu-sz:** Average queue size (queue depth)
- **inflight:** I/O operations currently in progress
- **Device-to-VM mapping:** Show which VMs use which disks

## Tips and Tricks

### Quick Storage Assessment

```
Storage healthy if:
✓ R_Lat < 1ms (SSD) or < 10ms (HDD)
✓ W_Lat < 2ms (SSD) or < 15ms (HDD)
✓ No device near max IOPS/throughput
```

### Find Busiest Disk

```
1. Press 's'
2. Press '3' to sort by R_MiB/s
3. Top entry is busiest for reads

OR

2. Press '4' to sort by W_MiB/s
3. Top entry is busiest for writes
```

### Identify Idle Disks

```
1. Press 's'
2. Scroll through list
3. Disks with 0.00 IOPS are unused
4. Consider consolidating VMs to reduce idle disks
```

## Troubleshooting

**Problem:** No devices showing

**Cause:** kvmtop filters loop and ram devices. Check:
```bash
cat /proc/diskstats
```

**Problem:** Unexpected high latency

**Causes:**
- Disk failure (check `dmesg` for errors)
- RAID rebuild in progress
- VM doing snapshot/backup
- Storage network congestion (iSCSI/NFS)

**Problem:** Metrics seem incorrect

**Verify with system tools:**
```bash
# Real-time I/O stats
iostat -x 1

# Detailed per-device stats
iotop -o

# Check disk errors
smartctl -a /dev/sda
```

## Limitations

- No per-VM disk attribution (shows device-level only)
- No queue depth or utilization % (planned for future)
- No historical trends (real-time only)

## Next Steps

- [Process View](process.md) - Find which VMs are I/O-bound
- [Network View](network.md) - Check network storage performance
- [Usage Guide](../usage.md) - Learn all keyboard shortcuts


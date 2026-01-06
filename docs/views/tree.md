# Tree View Documentation

The Tree View provides a hierarchical visualization of processes and their threads, essential for debugging multi-threaded applications and understanding vCPU behavior in virtual machines.

## Access

- **Keyboard:** Press `t` to toggle Tree View (in Process mode)
- **Return to flat view:** Press `t` again to toggle off

## Overview

Tree View enhances the Process View by showing:

1. **Parent process (TGID)** with aggregated metrics for all threads
2. **Individual threads (TIDs)** indented beneath, each with their own statistics

This is particularly useful for KVM/QEMU VMs, which are multi-threaded with:
- vCPU threads (one per virtual CPU)
- I/O threads
- Helper threads (VNC, migration, etc.)

## Display Format

```
PID       User     Uptime    ... CPU%  S  COMMAND
─────────────────────────────────────────────────────────
1234      root     02:15:33  ... 350.5 R  100 - web-vm
  └─ 1235 root     02:15:33  ... 100.0 R  [vCPU 0]
  └─ 1236 root     02:15:33  ... 100.0 R  [vCPU 1]
  └─ 1237 root     02:15:33  ... 100.0 R  [vCPU 2]
  └─ 1238 root     02:15:33  ...  50.5 R  [vCPU 3]
  └─ 1239 root     02:15:33  ...   0.0 S  [IO Thread]
```

### Interpretation

- **Main line (1234):** Aggregated metrics for entire process
  - CPU%: 350.5% = sum of all thread CPU usage
  - Shows VM name/ID
  
- **Thread lines (└─ prefix):** Individual thread metrics
  - Each thread shows its own CPU%, state, I/O
  - Indented for clarity
  - Thread names shown in brackets (when available)

## Use Cases

### 1. Identifying CPU Hotspots in VMs

**Problem:** VM reports high CPU usage, but you don't know which vCPU.

**Solution:**
```
1. Press 'c' for Process View
2. Press '2' to sort by CPU
3. Find your VM
4. Press 't' to enable Tree View
5. Look at vCPU thread CPU%
```

**Example:**
```
1234      ... 285.0 R  105 - database-vm
  └─ 1235 ... 100.0 R  [vCPU 0]    ← Saturated
  └─ 1236 ... 100.0 R  [vCPU 1]    ← Saturated
  └─ 1237 ...  75.0 R  [vCPU 2]    ← Moderate
  └─ 1238 ...  10.0 S  [vCPU 3]    ← Mostly idle
```

**Analysis:**
- vCPU 0 and 1 are fully utilized (100% each)
- Application is not efficiently using all 4 vCPUs
- **Action:** Investigate if application is single-threaded or has thread contention

### 2. Finding I/O Thread Bottlenecks

**Problem:** VM has high I/O wait, need to find which thread.

**Solution:**
```
1. Press 'c', sort by Wait ('5')
2. Find VM with high Wait
3. Press 't' for Tree View
4. Look for threads with high IO_Wait
```

**Example:**
```
1234      ... 1,250ms S  100 - file-server
  └─ 1235 ...     0ms S  [vCPU 0]
  └─ 1236 ...     0ms S  [vCPU 1]
  └─ 1237 ... 1,200ms D  [IO Thread 0]  ← Blocked on disk!
  └─ 1238 ...    50ms S  [IO Thread 1]
```

**Analysis:**
- IO Thread 0 is blocked (state 'D') with 1,200ms wait
- This thread is waiting for slow storage
- **Action:** Check Storage View ('s') for disk latency

### 3. Debugging Multi-Threaded Applications

**Problem:** Application using multiple threads, need to see per-thread behavior.

**Example:**
```
5678      ... 425.0 R  nginx
  └─ 5679 ...   5.0 S  [master]
  └─ 5680 ... 105.0 R  [worker 0]
  └─ 5681 ... 105.0 R  [worker 1]
  └─ 5682 ... 105.0 R  [worker 2]
  └─ 5683 ... 105.0 R  [worker 3]
```

**Analysis:**
- 1 master thread (low CPU)
- 4 worker threads (all at ~100% CPU)
- Load is well-distributed across workers
- **Conclusion:** Application is healthy and balanced

### 4. Detecting Idle Threads

**Example:**
```
9000      ...  50.5 R  100 - app-server
  └─ 9001 ...  50.0 R  [main thread]
  └─ 9002 ...   0.5 S  [logger thread]
  └─ 9003 ...   0.0 S  [monitor thread]
  └─ 9004 ...   0.0 S  [unused thread]
```

**Analysis:**
- Only main thread and logger doing work
- Two threads are completely idle
- **Possible optimization:** Reduce thread pool size

## Understanding Thread States

| State | Meaning | Typical Threads |
|-------|---------|-----------------|
| **R** | Running | Active vCPUs, worker threads |
| **S** | Sleeping | Idle threads, waiting for events |
| **D** | Disk Wait | I/O threads blocked on storage |
| **Z** | Zombie | Dead thread not yet reaped (rare, indicates bug) |

**Color coding (when enabled):**
- Regular states (R/S): Normal color
- 🔴 State 'D': Red (indicates I/O bottleneck)
- 🟡 State 'Z': Yellow (indicates bug)

## Thread Naming

kvmtop shows thread names when available:

### QEMU/KVM Threads

- `[vCPU 0]`, `[vCPU 1]`, ...: Virtual CPU threads
- `[IO Thread]`, `[IO Thread 0]`: I/O processing threads
- `[VNC server]`: VNC display thread
- `[migration]`: VM migration thread
- `[main-loop]`: Main event loop

### Generic Threads

For non-VM processes, thread names come from:
- `pthread_setname_np()` in the application
- Kernel-assigned names
- Command line (fallback)

If no name is available, threads show as `[tid]` where `tid` is the thread ID.

## Sorting in Tree View

Sorting applies to the **parent processes**, not individual threads:

```
Press '2' to sort by CPU:
  - Parents sorted by total CPU (sum of all threads)
  - Threads always listed under their parent
```

This means:
- A VM with 400% CPU (4 vCPUs at 100%) sorts before
- A VM with 150% CPU (1 vCPU at 150%)

## Performance Considerations

Tree View shows more data (parent + all threads), so:

**Pros:**
- Detailed per-thread visibility
- Essential for debugging

**Cons:**
- More screen space used
- More lines to scroll through
- Can be overwhelming on systems with many threads

**Tip:** Use filter (`/`) to focus on specific VMs when using Tree View.

## Metrics in Tree View

### Aggregated (Parent Line)

- **CPU%:** Sum of all thread CPU%
- **R_Log / W_Log:** Sum of all thread logical IOPS
- **Wait:** Sum of all thread I/O wait
- **Memory:** Shared across all threads (not summed)
- **State:** Usually 'S' or 'R' (most active thread state)

### Per-Thread (Thread Lines)

- **CPU%:** This thread's CPU usage only
- **R_Log / W_Log:** This thread's logical IOPS
- **Wait:** This thread's I/O wait
- **Memory:** (not shown, threads share memory)
- **State:** This thread's current state

## Examples

### Example 1: Balanced vCPU Usage

```
1000      ... 396.0 R  100 - balanced-vm
  └─ 1001 ...  99.0 R  [vCPU 0]
  └─ 1002 ...  99.0 R  [vCPU 1]
  └─ 1003 ...  99.0 R  [vCPU 2]
  └─ 1004 ...  99.0 R  [vCPU 3]
```

**Analysis:** Perfect! All vCPUs equally loaded. Application is well-threaded.

### Example 2: Unbalanced vCPU Usage

```
2000      ... 210.0 R  200 - unbalanced-vm
  └─ 2001 ... 100.0 R  [vCPU 0]
  └─ 2002 ... 100.0 R  [vCPU 1]
  └─ 2003 ...  10.0 S  [vCPU 2]
  └─ 2004 ...   0.0 S  [vCPU 3]
```

**Analysis:** Only 2 vCPUs active. Application is single or dual-threaded. Consider reducing vCPU count to match application concurrency.

### Example 3: I/O Bottleneck

```
3000      ... 1,850ms D  300 - io-heavy-vm
  └─ 3001 ...     0ms S  [vCPU 0]
  └─ 3002 ...     0ms S  [vCPU 1]
  └─ 3003 ... 1,800ms D  [IO Thread]  ← Problem here!
  └─ 3004 ...    50ms S  [IO Thread]
```

**Analysis:** IO Thread is blocked with 1,800ms wait (state 'D'). Storage is very slow.

### Example 4: Migration in Progress

```
4000      ... 185.0 R  400 - migrating-vm
  └─ 4001 ...  50.0 R  [vCPU 0]
  └─ 4002 ...  50.0 R  [vCPU 1]
  └─ 4003 ... 100.0 R  [migration]  ← Actively migrating
```

**Analysis:** Migration thread is active, consuming CPU to transfer VM state.

## Tips and Tricks

### Focus on Specific VM

```
1. Press 't' to enable Tree View
2. Press '/' for filter
3. Type VM name or ID
4. Only that VM and its threads shown
```

### Compare Thread Behavior

```
1. Enable Tree View
2. Sort by CPU ('2')
3. Compare vCPU thread distribution across VMs
4. Identify VMs with poor threading
```

### Quick vCPU Count

```
Tree View shows all vCPU threads
Count the "└─" lines with [vCPU] to see total vCPUs
```

## Limitations

- Thread names depend on application setting them
- Some applications don't name threads (shows as `[tid]`)
- Very large thread counts (100+) can make display unwieldy
- No thread CPU affinity shown (which host CPU core)

## Troubleshooting

**Problem:** Thread names show as `[1235]` instead of `[vCPU 0]`

**Cause:** QEMU didn't set thread names, or using old QEMU version.

**Solution:** Upgrade QEMU or check QEMU logs.

**Problem:** Tree View is too crowded

**Solution:**
1. Use filter (`/`) to show only specific processes
2. Reduce display limit (`l`) to show fewer parent processes
3. Toggle back to flat view (`t`)

**Problem:** Thread metrics seem wrong

**Cause:** Threads are being created/destroyed rapidly.

**Note:** kvmtop snapshots at each interval. Short-lived threads may not appear or show incomplete data.

## Next Steps

- [Process View](process.md) - Understanding parent process metrics
- [Usage Guide](../usage.md) - All keyboard shortcuts
- [Troubleshooting](../troubleshooting.md) - Common issues


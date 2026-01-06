# Troubleshooting Guide

Common issues and their solutions when using kvmtop.

## Installation Issues

### Binary Won't Execute

**Problem:** `bash: ./kvmtop: Permission denied`

**Solution:**
```bash
chmod +x kvmtop
```

**Problem:** `cannot execute binary file: Exec format error`

**Solution:** You're trying to run an x86_64 binary on a different architecture (ARM, 32-bit, etc.). kvmtop currently only supports x86_64 Linux systems.

### Build Failures

**Problem:** `gcc: command not found`

**Solution:**
```bash
# Debian/Ubuntu
sudo apt-get install build-essential

# RHEL/CentOS/Rocky
sudo yum install gcc make

# Alpine
sudo apk add gcc make musl-dev
```

**Problem:** Compilation errors during `make`

**Solution:**
1. Ensure you have a C99-compatible compiler
2. Check that all required headers are present
3. Try cleaning and rebuilding:
   ```bash
   make clean
   make
   ```

## Runtime Issues

### Missing I/O Statistics

**Problem:** Read/Write bandwidth shows 0.00, IO Wait is always 0

**Solution:** You're not running as root. I/O statistics in `/proc/[pid]/io` require root access:

```bash
sudo kvmtop
```

**What you'll see without root:**
- ✅ CPU usage, memory usage
- ✅ Process list, VM names
- ✅ Network statistics
- ❌ Read/write bytes
- ❌ I/O wait times
- ❌ Disk latency

### VM Names Not Showing

**Problem:** Processes show as `qemu-system-x86` instead of VM names

**Possible causes:**

1. **Non-standard QEMU parameters:** kvmtop parses `-name` and `-id` flags. If your setup uses different flags, VM names won't be detected.

   ```bash
   # Check your QEMU command line
   ps aux | grep qemu
   ```

2. **Custom wrappers:** If you use a wrapper script that doesn't pass through the `-name` flag, kvmtop can't extract the VM name.

3. **Libvirt with custom naming:** Some libvirt configurations use non-standard naming schemes.

**Workaround:** Use the filter feature (`/`) to search by PID instead.

### Display Issues

**Problem:** Flickering or garbled output

**Solutions:**

1. **Check terminal compatibility:**
   ```bash
   echo $TERM
   # Should be xterm, xterm-256color, or similar
   ```

2. **Set TERM variable:**
   ```bash
   export TERM=xterm-256color
   kvmtop
   ```

3. **Disable colors:**
   ```bash
   NO_COLOR=1 kvmtop
   ```

**Problem:** Output is too wide, columns are cut off

**Solutions:**

1. **Resize terminal:**
   - Make your terminal window wider
   - Reduce font size

2. **Check terminal width:**
   ```bash
   tput cols
   # kvmtop needs at least 100, recommends 120+
   ```

3. **Use a terminal multiplexer:**
   ```bash
   tmux
   kvmtop
   ```

**Problem:** Unicode characters display incorrectly

**Solution:** Ensure your terminal uses UTF-8 encoding:
```bash
echo $LANG
# Should contain UTF-8
export LANG=en_US.UTF-8
```

### Performance Issues

**Problem:** High CPU usage from kvmtop itself

**Causes:**
- Very short refresh interval (< 1 second)
- Thousands of processes on the system
- Frequent updates on busy systems

**Solutions:**

1. **Increase refresh interval:**
   ```bash
   kvmtop --interval 5.0  # or higher
   ```

2. **Reduce display limit:**
   ```bash
   kvmtop --limit 20
   ```

3. **Filter to specific processes:**
   ```bash
   kvmtop --pid 1234 --pid 5678
   ```

**Problem:** System slowdown while kvmtop is running

**Solution:** kvmtop scans `/proc` every refresh interval. On systems with 1000+ processes, this can cause noticeable load. Increase the interval:

```bash
kvmtop --interval 10.0
```

### Network View Issues

**Problem:** Network interfaces not showing VM names

**Cause:** kvmtop looks for `ifname=` parameters in QEMU command lines. If your setup uses different networking (macvtap, direct assignment), VM names won't be mapped.

**Workaround:** Look at the interface naming pattern:
- `tap105i0` → VM ID 105
- `vnet0` → First virtual network interface (check order)

**Problem:** `fw*` interfaces cluttering the view

**Solution:** kvmtop automatically filters out `fw*` (firewall bridge interfaces) and `lo` (loopback). If you see them, this is a bug.

### Storage View Issues

**Problem:** No devices showing in storage view

**Cause:** kvmtop filters out loop and ram devices. If you only have those, nothing will show.

**Check:**
```bash
cat /proc/diskstats
```

**Problem:** Latency values seem wrong

**Cause:** Latency is calculated from kernel counters. Very low or zero I/O can cause misleading values.

**Note:** Latency is per-operation average. A device with no I/O will show 0.00ms, which is correct.

## Common Errors

### `error: failed to open /proc`

**Cause:** Permission denied or `/proc` not mounted.

**Solution:**
```bash
# Check if /proc is mounted
mount | grep proc

# If not mounted (rare):
sudo mount -t proc proc /proc
```

### `error: out of memory`

**Cause:** System has thousands of processes/threads and kvmtop can't allocate enough memory.

**Solution:**
1. Use `--pid` to monitor specific processes only
2. Increase system memory
3. Close unnecessary processes

### `Segmentation fault`

**Cause:** Potential bug in kvmtop or corrupted `/proc` data.

**Solution:**
1. Report the bug with your system details
2. Try rebuilding with debug symbols:
   ```bash
   make clean
   CFLAGS="-g -O0" make
   ```
3. Run under gdb to get a backtrace:
   ```bash
   sudo gdb ./build/kvmtop
   (gdb) run
   # Wait for crash
   (gdb) bt
   ```

## SSH and Remote Access

**Problem:** kvmtop looks different over SSH

**Solution:** SSH terminal emulation can vary. Use a consistent TERM:

```bash
ssh -t user@host "TERM=xterm-256color kvmtop"
```

**Problem:** Colors don't work over SSH

**Solution:**
1. Check local terminal supports colors
2. Ensure SSH preserves TERM:
   ```bash
   # In ~/.ssh/config
   Host *
       SetEnv TERM=xterm-256color
   ```

## Platform-Specific Issues

### Proxmox VE

**Problem:** Permission denied even as root

**Cause:** Proxmox doesn't have permission issues typically, but if you're using unprivileged containers, you might not have access to host processes.

**Solution:** Run kvmtop on the Proxmox host, not inside a container.

### Alpine Linux

**Problem:** Binary doesn't work

**Cause:** Alpine uses musl libc instead of glibc. Make sure you're using the static binary or build from source.

**Solution:**
```bash
# Build on Alpine
apk add gcc make musl-dev
make
```

### WSL (Windows Subsystem for Linux)

**Problem:** kvmtop can't see any VMs

**Cause:** WSL doesn't run KVM virtual machines. kvmtop is designed for native Linux KVM hosts.

**Solution:** Run kvmtop on a native Linux system running KVM/QEMU.

## Getting Help

If your issue isn't covered here:

1. **Check kvmtop version:**
   ```bash
   kvmtop --version
   ```

2. **Gather system information:**
   ```bash
   uname -a
   cat /etc/os-release
   ```

3. **Enable debug output (if available):**
   ```bash
   # Run with strace to see system calls
   sudo strace -o /tmp/kvmtop.trace kvmtop
   ```

4. **Report the issue:**
   - Visit [GitHub Issues](https://github.com/yohaya/kvmtop/issues)
   - Include: OS version, kvmtop version, error messages, steps to reproduce

## Frequently Asked Questions

### Can kvmtop monitor Docker containers?

No, kvmtop is specifically designed for KVM/QEMU virtual machines. For Docker, use `docker stats` or `ctop`.

### Does kvmtop work with VirtualBox?

No, kvmtop is designed for KVM/QEMU only.

### Can I run kvmtop on the VM guest?

Yes, but it will only show processes within that guest, not other VMs on the host.

### How accurate is the CPU percentage?

Very accurate. It's calculated from `/proc/[pid]/stat` jiffies, the same source the kernel uses.

### Why does CPU% exceed 100%?

Each CPU core contributes 100%. A process using 2 cores fully will show 200%.

### Can kvmtop export data to a file?

Currently, kvmtop is designed for real-time interactive monitoring. For logging, use other tools like `vmstat`, `iostat`, or `sar`.

## Next Steps

- [Usage Guide](usage.md) - Learn all keyboard shortcuts
- [Configuration](configuration.md) - Customize kvmtop
- [Development](development.md) - Contributing to kvmtop


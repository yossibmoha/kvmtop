# Development Guide

Contributing to kvmtop development.

## Getting Started

### Prerequisites

- **C Compiler:** GCC 4.8+ or Clang 3.4+
- **Make:** GNU Make
- **Git:** For version control
- **Linux:** Development must be done on Linux (VM or native)

### Setting Up Development Environment

```bash
# Clone the repository
git clone https://github.com/yohaya/kvmtop.git
cd kvmtop

# Build
make

# Run from build directory
sudo ./build/kvmtop
```

## Project Structure

```
kvmtop/
├── src/
│   └── main.c          # Main source file (single-file project)
├── docs/               # Documentation
│   ├── index.md
│   ├── usage.md
│   └── ...
├── Makefile            # Build configuration
├── README.md           # Project overview
└── LICENSE             # GPL-3.0 license
```

## Build System

### Makefile Targets

```bash
# Build the release binary
make

# Build with custom version
make VERSION=v1.2.3

# Clean build artifacts
make clean

# Build with debug symbols
make CFLAGS="-g -O0 -Wall -Wextra"
```

### Build Flags

The default build uses:
- `-Wall -Wextra`: Enable warnings
- `-O2`: Optimization level 2
- `-static`: Create static binary

### Debug Build

For development and debugging:

```bash
# Build with debug symbols, no optimization
make clean
make CFLAGS="-g -O0 -Wall -Wextra"

# Run under gdb
sudo gdb ./build/kvmtop
(gdb) run
```

## Code Style

### C Style Guidelines

kvmtop follows these conventions:

1. **Indentation:** 4 spaces (no tabs)
2. **Braces:** K&R style
   ```c
   if (condition) {
       // code
   } else {
       // code
   }
   ```
3. **Naming:**
   - Functions: `snake_case`
   - Types: `snake_case_t`
   - Macros: `UPPER_CASE`
   - Variables: `snake_case`

4. **Comments:**
   - Use `//` for single-line comments
   - Use `/* */` for multi-line comments
   - Document complex algorithms

### Example

```c
typedef struct {
    int value;
    char name[32];
} my_struct_t;

static int calculate_value(int input) {
    if (input < 0) {
        return 0;
    }
    return input * 2;
}
```

## Architecture Overview

### Main Components

```
┌─────────────────────────────────────┐
│         main() entry point          │
└───────────────┬─────────────────────┘
                │
    ┌───────────┴───────────────┐
    │   Data Collection Loop    │
    │  (every refresh interval) │
    └───────────┬───────────────┘
                │
    ┌───────────┴────────────────┐
    │    Read /proc filesystem   │
    │  - Process stats (stat)    │
    │  - I/O stats (io)          │
    │  - Memory (statm)          │
    │  - Network (/proc/net/dev) │
    │  - Disks (diskstats)       │
    └───────────┬────────────────┘
                │
    ┌───────────┴────────────────┐
    │   Calculate Deltas         │
    │  (current - previous)      │
    └───────────┬────────────────┘
                │
    ┌───────────┴────────────────┐
    │   Sort & Filter Data       │
    └───────────┬────────────────┘
                │
    ┌───────────┴────────────────┐
    │   Display Output           │
    │  (based on selected view)  │
    └────────────────────────────┘
```

### Key Data Structures

```c
// Process/thread sample
typedef struct {
    pid_t pid;
    pid_t tgid;
    uint64_t key;
    
    // Raw counters
    uint64_t syscr, syscw;
    uint64_t read_bytes, write_bytes;
    uint64_t cpu_jiffies;
    uint64_t blkio_ticks;
    
    // Calculated metrics
    double cpu_pct;
    double r_iops, w_iops;
    double io_wait_ms;
    double r_mib, w_mib;
    
    char cmd[CMD_MAX];
    char user[32];
    char state;
} sample_t;

// Network interface
typedef struct {
    char name[32];
    uint64_t rx_bytes, tx_bytes;
    uint64_t rx_packets, tx_packets;
    
    double rx_mbps, tx_mbps;
    double rx_pps, tx_pps;
    
    int vmid;
    char vm_name[64];
} net_iface_t;

// Disk device
typedef struct {
    char name[32];
    unsigned long long rio, wio;
    unsigned long long rsect, wsect;
    
    double r_iops, w_iops;
    double r_mib, w_mib;
    double r_lat, w_lat;
} disk_sample_t;
```

## Adding Features

### Adding a New Metric

1. **Add field to structure:**
   ```c
   typedef struct {
       // Existing fields...
       uint64_t my_new_counter;
       double my_new_metric;
   } sample_t;
   ```

2. **Read from /proc:**
   ```c
   static int read_my_metric(pid_t pid, uint64_t *output) {
       char path[PATH_MAX];
       snprintf(path, sizeof(path), "/proc/%d/my_file", pid);
       
       FILE *f = fopen(path, "r");
       if (!f) return -1;
       
       fscanf(f, "%llu", output);
       fclose(f);
       return 0;
   }
   ```

3. **Calculate delta:**
   ```c
   // In main loop
   uint64_t delta = current->my_new_counter - previous->my_new_counter;
   current->my_new_metric = (double)delta / time_interval;
   ```

4. **Display:**
   ```c
   printf("%*.2f", width, sample->my_new_metric);
   ```

### Adding a New View

1. **Add to mode enum:**
   ```c
   typedef enum {
       MODE_PROCESS,
       MODE_NETWORK,
       MODE_STORAGE,
       MODE_MY_NEW_VIEW  // Add here
   } display_mode_t;
   ```

2. **Add keyboard shortcut:**
   ```c
   if (c == 'm' || c == 'M') {
       mode = MODE_MY_NEW_VIEW;
       dirty = 1;
   }
   ```

3. **Implement display logic:**
   ```c
   if (mode == MODE_MY_NEW_VIEW) {
       // Display headers
       printf("Header1  Header2  Header3\n");
       
       // Display data
       for (size_t i = 0; i < data_len; i++) {
           printf("%s  %d  %f\n", data[i].field1, ...);
       }
   }
   ```

## Testing

### Manual Testing Checklist

- [ ] Build succeeds without warnings
- [ ] Runs as root (full I/O stats visible)
- [ ] Runs as non-root (partial stats, no crash)
- [ ] All views (c, s, n, t) display correctly
- [ ] Sorting works in each view (1-8 keys)
- [ ] Interactive features work:
  - [ ] Filter (`/`)
  - [ ] Limit (`l`)
  - [ ] Refresh (`r`)
  - [ ] Freeze (`f`)
  - [ ] Help (`h`)
- [ ] Terminal resize handled gracefully
- [ ] No memory leaks (check with valgrind)

### Running Under Valgrind

```bash
# Build with debug symbols
make clean
make CFLAGS="-g -O0"

# Run with valgrind
sudo valgrind --leak-check=full --show-leak-kinds=all ./build/kvmtop
```

### Testing on Different Systems

Test on:
- Debian/Ubuntu (glibc)
- RHEL/CentOS/Rocky (glibc)
- Alpine (musl libc)
- Different kernel versions
- Systems with/without KVM VMs running

## Debugging Tips

### Print Debugging

```c
#ifdef DEBUG
    fprintf(stderr, "DEBUG: value=%d\n", my_value);
#endif
```

Build with:
```bash
make CFLAGS="-g -O0 -DDEBUG"
```

### GDB Basics

```bash
sudo gdb ./build/kvmtop
(gdb) break main
(gdb) run
(gdb) next          # Step over
(gdb) step          # Step into
(gdb) print var     # Print variable
(gdb) backtrace     # Show call stack
```

### Common Issues

**Problem:** Segfault when reading /proc files

**Check:**
- Buffer sizes (use `PATH_MAX`, check array bounds)
- NULL pointer checks
- `sscanf` return values

**Problem:** High CPU usage

**Profile:**
```bash
sudo perf record -g ./build/kvmtop
sudo perf report
```

## Contributing

### Workflow

1. **Fork the repository** on GitHub

2. **Create a feature branch:**
   ```bash
   git checkout -b feature/my-new-feature
   ```

3. **Make your changes:**
   - Follow code style guidelines
   - Add comments for complex logic
   - Test thoroughly

4. **Commit:**
   ```bash
   git add src/main.c
   git commit -m "Add feature: description"
   ```

5. **Push to your fork:**
   ```bash
   git push origin feature/my-new-feature
   ```

6. **Open a Pull Request** on GitHub

### Commit Message Guidelines

```
<type>: <short summary>

<detailed description>

<footer>
```

**Types:**
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation update
- `style:` Code style (formatting, no logic change)
- `refactor:` Code restructuring
- `perf:` Performance improvement
- `test:` Testing
- `chore:` Maintenance

**Example:**
```
feat: add disk utilization percentage to storage view

- Read io_ticks from /proc/diskstats
- Calculate utilization as percentage
- Display in storage view with color coding

Closes #42
```

### Pull Request Checklist

Before submitting:

- [ ] Code compiles without warnings
- [ ] Tested on at least one Linux distribution
- [ ] No memory leaks (valgrind clean)
- [ ] Documentation updated (if needed)
- [ ] Commit messages follow guidelines
- [ ] Code follows style guidelines

## License

kvmtop is licensed under GPL-3.0. All contributions must be compatible with this license.

By contributing, you agree that your contributions will be licensed under the GPL-3.0 license.

## Resources

- [/proc filesystem documentation](https://www.kernel.org/doc/html/latest/filesystems/proc.html)
- [Linux kernel documentation](https://www.kernel.org/doc/html/latest/)
- [ANSI escape codes](https://en.wikipedia.org/wiki/ANSI_escape_code)

## Next Steps

- [Usage Guide](usage.md) - Understand how kvmtop works
- [Troubleshooting](troubleshooting.md) - Debug issues
- [GitHub Repository](https://github.com/yohaya/kvmtop) - Source code


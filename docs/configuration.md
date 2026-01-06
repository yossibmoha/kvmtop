# Configuration Guide

kvmtop can be customized through command-line options and configuration files.

## Configuration File

kvmtop looks for a configuration file at `~/.kvmtoprc` on startup. If found, settings from this file will be used as defaults.

### File Location

```
~/.kvmtoprc
```

### File Format

The configuration file uses a simple KEY=VALUE format:

```ini
# kvmtop configuration file
# Lines starting with # are comments

# Refresh interval in seconds (default: 5.0)
interval=2.0

# Number of entries to display (default: 50)
limit=100

# Enable color output (on/off, default: on)
color=on

# Default sort column (pid, cpu, wait, rmib, wmib, default: cpu)
default_sort=cpu

# Default view mode (process, network, storage, default: process)
default_mode=process
```

### Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `interval` | float | 5.0 | Refresh interval in seconds |
| `limit` | integer | 50 | Number of entries to display |
| `color` | on/off | on | Enable ANSI color coding |
| `default_sort` | string | cpu | Default sort column in process view |
| `default_mode` | string | process | Default view on startup |

### Example Configurations

#### High-Frequency Monitoring

```ini
# Fast refresh for real-time monitoring
interval=1.0
limit=20
color=on
default_sort=cpu
```

#### Large Display

```ini
# Show more entries on large screens
interval=5.0
limit=200
color=on
```

#### Troubleshooting Mode

```ini
# Focus on I/O bottlenecks
interval=2.0
limit=50
default_sort=wait
default_mode=process
```

## Command-Line Override

Command-line options always override configuration file settings:

```bash
# Config file says interval=5.0, but this overrides to 2.0
kvmtop --interval 2.0
```

## Color Coding

kvmtop uses ANSI colors to highlight critical values:

### Color Scheme

| Color | CPU % | IO Wait (ms) | Process State |
|-------|-------|--------------|---------------|
| 🟢 Green | < 80% | < 500ms | Normal (R/S) |
| 🟡 Yellow | 80-95% | 500-1000ms | Zombie (Z) |
| 🔴 Red | > 95% | > 1000ms | Disk Wait (D) |

### Disabling Colors

If colors don't display correctly in your terminal:

```ini
# In ~/.kvmtoprc
color=off
```

Or set the environment variable:

```bash
NO_COLOR=1 kvmtop
```

## Terminal Compatibility

kvmtop works best with terminals that support:

- ANSI escape codes
- UTF-8 encoding
- At least 120 columns width

### Recommended Terminals

- **Linux:** gnome-terminal, konsole, xterm
- **macOS:** iTerm2, Terminal.app
- **Windows:** Windows Terminal, WSL2 terminals
- **SSH:** Any terminal with `TERM=xterm-256color`

### Terminal Width

kvmtop adapts to your terminal width. For best results:

- **Minimum:** 100 columns
- **Recommended:** 120+ columns
- **Optimal:** 150+ columns

Check your terminal width:

```bash
tput cols
```

Resize your terminal or font size if the output looks cramped.

## Environment Variables

| Variable | Effect | Example |
|----------|--------|---------|
| `TERM` | Terminal type detection | `export TERM=xterm-256color` |
| `NO_COLOR` | Disable colors | `export NO_COLOR=1` |
| `COLUMNS` | Override terminal width | `export COLUMNS=150` |

## Permissions and Access

### Running as Root

For full functionality, kvmtop requires root privileges:

```bash
sudo kvmtop
```

### Capability-Based Access (Alternative to sudo)

Instead of full root, you can grant specific capabilities:

```bash
# Grant capability to read /proc files
sudo setcap cap_sys_ptrace=eip /usr/local/bin/kvmtop

# Now you can run without sudo (partial functionality)
kvmtop
```

> **Note:** This still won't give access to all I/O statistics. Root is recommended.

### SELinux Considerations

On SELinux systems, you may need to allow kvmtop to read proc files:

```bash
# Check for denials
sudo ausearch -m avc -ts recent | grep kvmtop

# Create a policy if needed (consult your SELinux documentation)
```

## Performance Tuning

### Refresh Interval

The refresh interval affects:

- **CPU overhead:** Lower interval = more overhead
- **Responsiveness:** Lower interval = more real-time
- **Accuracy:** Lower interval = better precision

**Guidelines:**

| Interval | Use Case | CPU Overhead |
|----------|----------|--------------|
| 1.0s | Troubleshooting, debugging | High |
| 2.0s | Active monitoring | Medium |
| 5.0s | Standard monitoring (default) | Low |
| 10.0s+ | Low-overhead background | Very Low |

### Display Limit

Showing more entries increases:
- Screen space used
- Terminal rendering time
- Scrollback buffer

**Recommendations:**
- **Default (50):** Good for most uses
- **20-30:** Focus on top consumers
- **100+:** Large screens, comprehensive view

## Default Configuration

If no configuration file exists, kvmtop uses these defaults:

```ini
interval=5.0
limit=50
color=on
default_sort=cpu
default_mode=process
```

## Creating a Configuration File

```bash
# Create the file
cat > ~/.kvmtoprc << 'EOF'
# My kvmtop configuration
interval=2.0
limit=100
color=on
default_sort=cpu
default_mode=process
EOF

# Test it
kvmtop
```

## Troubleshooting Configuration

### Config File Not Being Read

```bash
# Check if file exists
ls -la ~/.kvmtoprc

# Check file permissions (should be readable)
chmod 644 ~/.kvmtoprc

# Check file format (no extra spaces, correct syntax)
cat ~/.kvmtoprc
```

### Settings Not Taking Effect

1. Check command-line options (they override config file)
2. Verify correct key names (case-sensitive)
3. Check for syntax errors in config file
4. Try running with explicit options to test

## Next Steps

- [Usage Guide](usage.md) - Learn keyboard shortcuts
- [Troubleshooting](troubleshooting.md) - Common issues
- [Views Documentation](views/process.md) - Understanding the output


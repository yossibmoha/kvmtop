# Changelog

All notable changes to kvmtop will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Mouse support for sorting** - click column headers to sort (SGR extended mouse mode)
- Help screen overlay (`h` key) with complete keyboard shortcut reference
- Visual sort column indicator (`*` mark on active sort column)
- Page fault tracking (minor and major faults per second)
- Disk utilization percentage, queue depth, and inflight I/O in storage view
- ANSI color coding for critical values (CPU, I/O wait, process state)
- Infrastructure for VM-to-disk mapping
- `--version` / `-v` command-line flag to show version information
- Comprehensive documentation in `docs/` folder
- Configuration file support (`~/.kvmtoprc`)

### Changed
- Enhanced storage view with additional metrics
- Improved help documentation
- Updated README with links to detailed documentation

### Fixed
- Terminal handling edge cases
- Memory initialization issues

## [1.0.1] - 2024-01-XX

### Added
- Storage view (`s` key) showing block device I/O statistics
- Disk latency tracking (read/write latency per operation)
- Support for multiple sorting options in storage view
- Queue depth and in-flight I/O tracking

### Changed
- Improved process collection to include all threads
- Enhanced VM name detection from QEMU command lines
- Better handling of terminal resize events

### Fixed
- Memory leak in vector reallocation
- Incorrect I/O wait calculation for multi-threaded processes
- Network interface filtering for firewall bridges

## [1.0.0] - 2023-12-XX

### Added
- Initial release of kvmtop
- Process view with CPU, memory, and I/O metrics
- Network view with interface statistics
- Tree view for thread hierarchy
- Interactive filtering by PID, name, user, or VM
- Customizable refresh interval and display limit
- Freeze/resume functionality
- Auto-discovery of KVM VMs from QEMU command lines
- Network interface to VM mapping
- Multiple sort options for all views
- Static binary compilation for maximum portability

### Features
- Zero dependencies - single static binary
- Real-time monitoring with configurable refresh intervals
- Multi-view dashboard (Process, Tree, Network, Storage)
- I/O latency and bottleneck detection
- VM-aware monitoring with auto-detection
- Interactive keyboard controls
- Terminal-adaptive display

## Version History Overview

| Version | Release Date | Key Features |
|---------|--------------|--------------|
| 1.0.0 | 2023-12 | Initial release, basic monitoring |
| 1.0.1 | 2024-01 | Storage view, enhanced metrics |
| Unreleased | TBD | Mouse support, help screen, colors, config file |

## Upgrade Notes

### Upgrading to Unreleased Version

- **Mouse support** - click column headers to sort (requires terminal with mouse support)
- New `~/.kvmtoprc` configuration file support (optional)
- Color output is enabled by default (disable with `color=off` in config)
- Help screen available with `h` key
- No breaking changes to command-line interface

### Upgrading to 1.0.1

- No breaking changes
- New storage view accessible with `s` key
- All existing functionality preserved

### Upgrading to 1.0.0

- Initial release, no upgrade path

## Deprecation Notices

None currently.

## Security

### Reporting Security Issues

If you discover a security vulnerability, please email security@example.com instead of using the public issue tracker.

### Security Considerations

- kvmtop requires root access for full functionality
- Access to `/proc` filesystem can expose sensitive system information
- Run only on trusted systems
- Review the source code before running as root

## Compatibility

### Supported Systems

- **Linux Kernel:** 2.6.26+
- **Architecture:** x86_64 only
- **C Library:** glibc, musl (static build)
- **Distributions:** All major Linux distributions

### Tested Platforms

- ✅ Debian 10, 11, 12
- ✅ Ubuntu 18.04, 20.04, 22.04
- ✅ RHEL/CentOS 7, 8, 9
- ✅ Rocky Linux 8, 9
- ✅ Alpine Linux 3.15+
- ✅ Proxmox VE 7, 8

### Known Limitations

- x86_64 architecture only (no ARM, 32-bit support)
- Designed for KVM/QEMU only (not VirtualBox, VMware, etc.)
- Requires Linux kernel 2.6.26+ for `/proc` interface
- Color support requires ANSI-compatible terminal
- Mouse support requires terminal with SGR mouse mode (most modern terminals)

## Future Plans

Planned features for upcoming releases:

### v1.1.0
- Process signal support (kill/stop processes)
- CSV/JSON export functionality
- Enhanced VM-to-disk mapping
- Historical data tracking (sparklines)

### v1.2.0
- NUMA topology awareness
- Hugepage usage tracking
- Container/cgroup detection
- ~~Mouse support for terminal interaction~~ ✅ (implemented in Unreleased)

### v2.0.0
- Multi-host monitoring (SSH aggregation)
- Web interface option
- Alerting/threshold notifications
- Plugin system for extensibility

See [GitHub Issues](https://github.com/yohaya/kvmtop/issues) for detailed feature discussions.

## Contributors

Thank you to all contributors who have helped improve kvmtop!

- Lead Developer: [Your Name]
- Contributors: See [GitHub Contributors](https://github.com/yohaya/kvmtop/graphs/contributors)

## License

kvmtop is released under the GNU General Public License v3.0. See [LICENSE](../LICENSE) for details.


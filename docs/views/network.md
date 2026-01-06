# Network View Documentation

The Network View displays real-time network interface statistics with automatic VM mapping for virtual machine monitoring.

## Access

- **Keyboard:** Press `n` to switch to Network View
- **From other views:** Press `n` at any time

## Overview

Network View shows statistics for all network interfaces on the system, with special focus on virtual machine network devices. It automatically:

- Maps TAP interfaces to VMs (e.g., `tap105i0` → VM 105)
- Filters out noise (loopback, firewall bridges)
- Shows top 50 busiest interfaces
- Sorts by transmit rate by default

## Column Reference

| Column | Full Name | Unit | Description |
|--------|-----------|------|-------------|
| **IFACE** | Interface Name | - | Network interface name (e.g., `eth0`, `tap100i0`, `ens18`). |
| **STATE** | Link State | - | Interface state: `up`, `down`, `unknown`. Only `up` interfaces carry traffic. |
| **RX_Mbps** | Receive Rate | Mbps | Incoming traffic in megabits per second. Click `1` to sort by RX. |
| **TX_Mbps** | Transmit Rate | Mbps | Outgoing traffic in megabits per second. Click `2` to sort by TX (default). |
| **RX_Pkts** | Receive Packets | pps | Incoming packets per second. High PPS with low Mbps = small packet traffic. |
| **TX_Pkts** | Transmit Packets | pps | Outgoing packets per second. |
| **RX_Err** | Receive Errors | err/s | Receive errors per second. Should be 0; if > 0, investigate. |
| **TX_Err** | Transmit Errors | err/s | Transmit errors per second. Should be 0; if > 0, investigate. |
| **VMID** | Virtual Machine ID | - | ID of the VM using this interface (e.g., `100`). Shows `-` if not a VM interface. |
| **VM_NAME** | Virtual Machine Name | - | Name of the VM (e.g., `database-vm`). Shows blank if not detected. |

## Sorting Options

| Key | Sort By | Description |
|-----|---------|-------------|
| `1` | RX_Mbps | Sort by receive rate (incoming traffic) |
| `2` | TX_Mbps | Sort by transmit rate (outgoing traffic, default) |

**Note:** Sorting toggles between ascending/descending with repeated presses.

## Interface Types

### Physical Interfaces

- `eth0`, `eth1`, `ens18`, `enp1s0`: Physical network cards
- **Use:** Host system network traffic
- **VMID:** Shows `-` (not VM-specific)

### Virtual Machine Interfaces

- `tap100i0`, `tap105i0`: TAP interfaces for KVM VMs
- `vnet0`, `vnet1`: Libvirt virtual network interfaces
- **Use:** VM network traffic
- **VMID:** Automatically detected (e.g., `105` from `tap105i0`)
- **VM_NAME:** Parsed from QEMU command line

### Bridge Interfaces

- `vmbr0`, `br0`: Network bridges
- **Use:** Connecting VMs to physical network
- **Note:** Shows total traffic across all bridged VMs

### Automatically Filtered

kvmtop hides these by default:
- `lo`: Loopback (127.0.0.1 traffic)
- `fw*`: Proxmox firewall bridge interfaces

## Understanding the Metrics

### Bandwidth (Mbps)

- **RX_Mbps:** Data coming INTO the interface
- **TX_Mbps:** Data going OUT OF the interface

**For VMs:**
- RX = Downloads, incoming connections
- TX = Uploads, outgoing connections

**Unit conversion:**
- 1 Mbps = 0.125 MB/s
- 1000 Mbps = 1 Gbps
- Example: 125 Mbps = ~15.6 MB/s

### Packets Per Second (PPS)

PPS measures packet count, not size.

**Normal traffic:**
- HTTP/HTTPS: ~100-1000 PPS at moderate load
- Database queries: ~500-5000 PPS
- File transfer: Lower PPS, high Mbps

**Abnormal patterns:**

| Pattern | RX_Mbps | TX_Mbps | RX_Pkts | TX_Pkts | Likely Cause |
|---------|---------|---------|---------|---------|--------------|
| Small packet flood | Low | Low | Very High | Normal | DDoS, DNS amplification |
| Large packet flood | Very High | High | Moderate | Moderate | DDoS, bandwidth exhaustion |
| Port scan | Low | Low | High | Low | Network scanning attack |
| Idle | 0 | 0 | 0 | 0 | VM/interface not in use |

**Interpretation:**
- **High PPS, Low Mbps:** Small packets (e.g., 64-byte packets from DDoS, DNS)
- **Low PPS, High Mbps:** Large packets (e.g., file transfer, video streaming)

### Error Rates

**RX_Err / TX_Err should always be 0 or very close to 0.**

**If errors > 0:**

| Error Type | Possible Cause | Action |
|------------|----------------|--------|
| **RX errors** | Bad cable, NIC issues, duplex mismatch | Check physical connections |
| **TX errors** | Overloaded NIC, driver bug, resource exhaustion | Check NIC load, update drivers |
| **Both** | Hardware failure, driver incompatibility | Replace hardware, update firmware |

**Check with:**
```bash
ethtool -S eth0   # Detailed error stats
ip -s link show   # Link statistics
```

## VM Network Mapping

### How It Works

kvmtop parses QEMU command lines to find:

```bash
# QEMU command line
-netdev tap,id=net0,ifname=tap105i0,script=...
-name database-vm
-id 105
```

From this, kvmtop extracts:
- Interface: `tap105i0`
- VM ID: `105`
- VM Name: `database-vm`

### Naming Patterns

| Pattern | VM ID | Example |
|---------|-------|---------|
| `tap<ID>i0` | `<ID>` | `tap105i0` → VM 105 |
| `tap<ID>i1` | `<ID>` | `tap100i1` → VM 100 (2nd NIC) |
| `vnet<N>` | Detected from proc | `vnet0` → (lookup by PID) |

### When VM Names Don't Show

**Reasons:**
1. Non-standard QEMU parameters (custom wrappers)
2. VM not started with `-name` flag
3. Complex libvirt configuration
4. Custom network setup (not using tap interfaces)

**Workaround:** Use the filter feature to match by interface name pattern.

## Use Cases

### Find Network-Intensive VMs

```
1. Press 'n' for Network View
2. Already sorted by TX (default)
3. Top entries are VMs sending most data
```

### Identify Inbound Traffic Leaders

```
1. Press 'n' for Network View
2. Press '1' to sort by RX
3. Top entries receiving most data
```

### Detect Network Attacks

**DDoS Detection:**
```
1. Press 'n'
2. Look for unusually high RX_Pkts
3. Compare RX_Pkts vs RX_Mbps ratio
4. High PPS + Low Mbps = small packet flood
```

**Example:**
```
tap105i0  up  12.5  5.2  125,000  8,000  0.0  0.0  105  web-vm
```
- 125K PPS but only 12.5 Mbps
- Average packet size: ~12.5 / 125 = 0.1 Mbits = 12.8 bytes
- **Indication:** Likely under attack (normal packets are 64+ bytes)

### Monitor Specific VM Network

```
1. Press 'n'
2. Press '/' for filter
3. Type VM ID or name (e.g., "105" or "web-vm")
4. Press Enter
```

### Check for Errors

```
1. Press 'n'
2. Scan RX_Err and TX_Err columns
3. Any non-zero value needs investigation
```

## Network Bottleneck Detection

### Signs of Network Bottleneck

1. **Interface at line rate:**
   - 1 Gbps NIC showing ~950 Mbps consistently
   - 10 Gbps NIC showing ~9.5 Gbps consistently

2. **High error rates:**
   - RX_Err or TX_Err > 0 and increasing

3. **Packet loss:**
   - Check with: `ping -c 100 <host>` (should be 0% loss)

### Resolution Steps

1. **Upgrade network:**
   - 1 Gbps → 10 Gbps
   - 10 Gbps → 40/100 Gbps

2. **Load balance:**
   - Multiple NICs per VM
   - Distribute VMs across multiple physical hosts

3. **QoS/Traffic shaping:**
   - Limit bandwidth per VM
   - Prioritize critical traffic

## Tips and Tricks

### Quick Network Health Check

```
1. Press 'n'
2. Look for:
   ✓ All interfaces STATE = up (expected)
   ✓ Error columns = 0.0
   ✓ Expected VMs showing traffic
   ✓ No unexpected high traffic
```

### Find Idle VMs

```
1. Press 'n'
2. Press '2' to sort by TX (or '1' for RX)
3. Scroll to bottom
4. VMs with 0.00 Mbps are idle
```

### Compare VM Network Usage

```
1. Press 'n'
2. Note TX_Mbps for each VM
3. Identify outliers (much higher/lower than average)
```

### Monitor During Migration

```
1. Start VM migration
2. Press 'n' in kvmtop
3. Watch for:
   - Source VM: TX_Mbps increases (sending state)
   - Destination VM: RX_Mbps increases (receiving state)
```

## Limitations

1. **Top 50 only:** Currently shows top 50 interfaces (hardcoded)
2. **No historical data:** Real-time only, no graphs
3. **No connection tracking:** Shows aggregate, not per-connection
4. **Limited VM detection:** Only standard QEMU/libvirt setups

## Troubleshooting

**Problem:** VM names not showing

**Solution:** Check QEMU command line has `-name` parameter:
```bash
ps aux | grep qemu | grep "name database-vm"
```

**Problem:** Interface not listed

**Solution:** 
- Check if filtered (loopback, firewall bridges)
- Check if in top 50 by traffic

**Problem:** Metrics seem wrong

**Solution:** Compare with system tools:
```bash
# Real-time stats
watch -n 1 'cat /proc/net/dev'

# Or use iftop
iftop -i eth0
```

## Next Steps

- [Process View](process.md) - Find CPU/I/O bottlenecks
- [Storage View](storage.md) - Analyze disk performance
- [Usage Guide](../usage.md) - Learn all features


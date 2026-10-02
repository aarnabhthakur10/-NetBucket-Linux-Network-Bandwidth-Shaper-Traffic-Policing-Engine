# TUN/TAP — Linux Virtual Network Interfaces

## What is TUN/TAP?

TUN and TAP are **virtual network interfaces** provided by the Linux kernel.

Unlike a physical NIC (which connects to a wire), a TUN/TAP device connects
a network interface directly to a **user-space process** via a file descriptor.

## TUN vs TAP

| Property | TUN | TAP |
|---|---|---|
| OSI Layer | Layer 3 (Network) | Layer 2 (Data Link) |
| Packet format | Raw IP packets | Ethernet frames (with MAC header) |
| Use case | VPNs, tunnels, traffic shapers | Bridges, virtual switches, VMs |
| Overhead | Less (no MAC header) | More (Ethernet header = 14 bytes) |

**NetBucket uses TUN** because:
- We process IP packets (not Ethernet frames)
- iperf3, ping, and TCP/UDP traffic is IP-level
- No MAC address management needed
- TUN is standard for this type of traffic engineering

## How TUN works

```
User space (NetBucket)        Linux Kernel
        │                          │
  open("/dev/net/tun")  ←──── /dev/net/tun (clone device)
        │                          │
  ioctl(TUNSETIFF)      ──────────► Creates "netbucket0" interface
        │                          │
  read(fd, buf)          ←──────── Kernel delivers arriving IP packet
        │                          │
  write(fd, buf)         ──────────► Kernel receives IP packet from user-space
```

## Key ioctl flags

```c
struct ifreq ifr;
strncpy(ifr.ifr_name, "netbucket0", IFNAMSIZ - 1);
ifr.ifr_flags = IFF_TUN      // TUN mode (not TAP)
              | IFF_NO_PI;   // No packet info prefix
```

### IFF_NO_PI explained

Without `IFF_NO_PI`, each `read()` call returns:
```
[flags 2 bytes] [proto 2 bytes] [raw IP packet bytes]
```

With `IFF_NO_PI`, each `read()` returns:
```
[raw IP packet bytes]
```

We use `IFF_NO_PI` because we parse the IP header directly — we don't
need the kernel's redundant metadata prefix.

## Permissions

Opening `/dev/net/tun` requires `CAP_NET_ADMIN` capability.

```bash
# Run as root:
sudo ./build/netbucket daemon

# Or grant the capability to the binary:
sudo setcap cap_net_admin+ep ./build/netbucket
```

## Packet flow diagram

```
iperf3 client (netns A)
        │ IP packet
        ▼
Linux routing → netbucket0 interface
                        │ kernel → user-space
                        ▼
                TunDevice::read_packet()
                        │
                        ▼
                Packet::from_bytes()  (parses IP header)
                        │
                        ▼
                TrafficController::process()
                        │
                  [allowed]
                        │
                        ▼
                TunDevice::write_packet()
                        │ user-space → kernel
                        ▼
                Linux routing → netbucket_out interface
                        │
                        ▼
                iperf3 server (netns B)
```

## Virtual Network Setup

```
netbucket-client namespace         netbucket-server namespace
        │                                    │
   veth-client (192.168.100.1)      veth-server (192.168.100.2)
        │                                    │
        └─────────── connected ──────────────┘
                   (veth pair)
```

The shaper sits in the host namespace and reads/writes the TUN interface
that is reachable from the client namespace.

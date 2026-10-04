#!/usr/bin/env bash
# scripts/setup_network.sh
#
# NetBucket — Virtual Network Setup Script
#
# Creates an isolated Linux virtual network environment for testing:
#
#   Network Namespace "client"
#          |
#        veth0 (192.168.100.1/24)
#          |
#        veth1 (192.168.100.2/24)
#          |
#   Network Namespace "server"
#
# The traffic shaper will eventually sit between the two namespaces
# using a TUN interface.
#
# REQUIREMENTS:
#   - Linux (Ubuntu 22.04+ recommended)
#   - Run as root or with CAP_NET_ADMIN
#
# USAGE:
#   sudo ./scripts/setup_network.sh
#
# CLEANUP:
#   sudo ./scripts/cleanup_network.sh
#
# WARNING:
#   This script only creates virtual interfaces in network namespaces.
#   It does NOT modify your host's default routing or firewall.
#   It is safe to run on a machine with a live Internet connection.

set -euo pipefail

# Configuration 
NS_CLIENT="netbucket-client"
NS_SERVER="netbucket-server"
VETH_CLIENT="veth-client"
VETH_SERVER="veth-server"
IP_CLIENT="192.168.100.1"
IP_SERVER="192.168.100.2"
PREFIX="24"

echo "[netbucket] Setting up virtual network..."

#  Create network namespaces 
echo "[netbucket] Creating network namespaces: $NS_CLIENT, $NS_SERVER"
ip netns add "$NS_CLIENT"
ip netns add "$NS_SERVER"

#  Create veth pair 
echo "[netbucket] Creating veth pair: $VETH_CLIENT <-> $VETH_SERVER"
ip link add "$VETH_CLIENT" type veth peer name "$VETH_SERVER"

#  Assign interfaces to namespaces 
echo "[netbucket] Assigning interfaces to namespaces"
ip link set "$VETH_CLIENT" netns "$NS_CLIENT"
ip link set "$VETH_SERVER" netns "$NS_SERVER"

#  Configure IP addresses 
echo "[netbucket] Configuring IP addresses"
ip netns exec "$NS_CLIENT" ip addr add "${IP_CLIENT}/${PREFIX}" dev "$VETH_CLIENT"
ip netns exec "$NS_SERVER" ip addr add "${IP_SERVER}/${PREFIX}" dev "$VETH_SERVER"

# Bring interfaces up 
echo "[netbucket] Bringing interfaces up"
ip netns exec "$NS_CLIENT" ip link set "$VETH_CLIENT" up
ip netns exec "$NS_CLIENT" ip link set lo up
ip netns exec "$NS_SERVER" ip link set "$VETH_SERVER" up
ip netns exec "$NS_SERVER" ip link set lo up

#  Verify 
echo "[netbucket] Verifying connectivity..."
if ip netns exec "$NS_CLIENT" ping -c 1 -W 2 "$IP_SERVER" &>/dev/null; then
    echo "[netbucket] ✓ Connectivity verified: $IP_CLIENT -> $IP_SERVER"
else
    echo "[netbucket] ✗ WARNING: ping failed. Check interface setup."
fi

echo "[netbucket] Virtual network ready."
echo ""
echo "  Client namespace:  $NS_CLIENT  ($IP_CLIENT)"
echo "  Server namespace:  $NS_SERVER  ($IP_SERVER)"
echo ""
echo "  To run a command in the client namespace:"
echo "    sudo ip netns exec $NS_CLIENT <command>"
echo ""
echo "  To run a command in the server namespace:"
echo "    sudo ip netns exec $NS_SERVER <command>"
echo ""
echo "  To clean up:"
echo "    sudo ./scripts/cleanup_network.sh"

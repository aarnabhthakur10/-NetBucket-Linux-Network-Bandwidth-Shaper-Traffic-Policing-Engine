#!/usr/bin/env bash


set -euo pipefail


NS_CLIENT="netbucket-client"
NS_SERVER="netbucket-server"
VETH_CLIENT="veth-client"
VETH_SERVER="veth-server"
IP_CLIENT="192.168.100.1"
IP_SERVER="192.168.100.2"
PREFIX="24"

echo "[netbucket] Setting up virtual network..."


echo "[netbucket] Creating network namespaces: $NS_CLIENT, $NS_SERVER"
ip netns add "$NS_CLIENT"
ip netns add "$NS_SERVER"


echo "[netbucket] Creating veth pair: $VETH_CLIENT <-> $VETH_SERVER"
ip link add "$VETH_CLIENT" type veth peer name "$VETH_SERVER"


echo "[netbucket] Assigning interfaces to namespaces"
ip link set "$VETH_CLIENT" netns "$NS_CLIENT"
ip link set "$VETH_SERVER" netns "$NS_SERVER"


echo "[netbucket] Configuring IP addresses"
ip netns exec "$NS_CLIENT" ip addr add "${IP_CLIENT}/${PREFIX}" dev "$VETH_CLIENT"
ip netns exec "$NS_SERVER" ip addr add "${IP_SERVER}/${PREFIX}" dev "$VETH_SERVER"


echo "[netbucket] Bringing interfaces up"
ip netns exec "$NS_CLIENT" ip link set "$VETH_CLIENT" up
ip netns exec "$NS_CLIENT" ip link set lo up
ip netns exec "$NS_SERVER" ip link set "$VETH_SERVER" up
ip netns exec "$NS_SERVER" ip link set lo up


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

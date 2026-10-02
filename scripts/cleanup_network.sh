#!/usr/bin/env bash
# scripts/cleanup_network.sh
#
# NetBucket — Virtual Network Cleanup Script
#
# Removes everything created by setup_network.sh:
#   - network namespaces: netbucket-client, netbucket-server
#   - veth interfaces (automatically removed with namespaces)
#
# USAGE:
#   sudo ./scripts/cleanup_network.sh
#
# It is safe to run this even if setup partially failed.
# Each step is attempted independently.

set -uo pipefail   # Note: no -e, so we continue even if one step fails

NS_CLIENT="netbucket-client"
NS_SERVER="netbucket-server"

echo "[netbucket] Cleaning up virtual network..."

# ─── Remove namespaces ────────────────────────────────────────────────────────
# Deleting a namespace automatically removes all veth interfaces inside it.

if ip netns list | grep -q "$NS_CLIENT"; then
    echo "[netbucket] Removing namespace: $NS_CLIENT"
    ip netns del "$NS_CLIENT" && echo "[netbucket] ✓ Removed $NS_CLIENT" \
                              || echo "[netbucket] ✗ Failed to remove $NS_CLIENT"
else
    echo "[netbucket] Namespace $NS_CLIENT does not exist (already clean)"
fi

if ip netns list | grep -q "$NS_SERVER"; then
    echo "[netbucket] Removing namespace: $NS_SERVER"
    ip netns del "$NS_SERVER" && echo "[netbucket] ✓ Removed $NS_SERVER" \
                              || echo "[netbucket] ✗ Failed to remove $NS_SERVER"
else
    echo "[netbucket] Namespace $NS_SERVER does not exist (already clean)"
fi

echo "[netbucket] Cleanup complete."
echo "[netbucket] Run 'ip netns list' to verify all namespaces are removed."

#!/usr/bin/env bash


set -uo pipefail

NS_CLIENT="netbucket-client"
NS_SERVER="netbucket-server"

echo "[netbucket] Cleaning up virtual network..."



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

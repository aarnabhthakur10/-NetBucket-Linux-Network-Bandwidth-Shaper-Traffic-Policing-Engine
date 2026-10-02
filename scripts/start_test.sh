#!/usr/bin/env bash
# scripts/start_test.sh
#
# NetBucket — iperf3 Traffic Test
#
# PREREQUISITES:
#   1. sudo ./scripts/setup_network.sh   (creates the namespaces)
#   2. ./build/netbucket daemon &        (start the shaper — requires TUN)
#
# USAGE:
#   sudo ./scripts/start_test.sh [RATE_MBPS] [DURATION_SEC]
#
# EXAMPLES:
#   sudo ./scripts/start_test.sh 10 30    # Generate 50 Mbps, shaper at 10 Mbps, run 30s
#   sudo ./scripts/start_test.sh 20 10
#
# WHAT THIS DEMONSTRATES:
#   Client (namespace netbucket-client) sends 50 Mbps to server.
#   The NetBucket shaper allows only RATE_MBPS through.
#   Server-side iperf3 measures the actual received throughput.

set -euo pipefail

RATE_MBPS=${1:-10}
DURATION=${2:-30}
GENERATE_MBPS=50    # Always generate 50 Mbps — 5× the typical limit

NS_CLIENT="netbucket-client"
NS_SERVER="netbucket-server"
IP_SERVER="192.168.100.2"
IPERF_PORT=5201

echo ""
echo "══════════════════════════════════════════════════════════"
echo "  NetBucket Traffic Test"
echo "  Generating:  ${GENERATE_MBPS} Mbps"
echo "  Limit:        ${RATE_MBPS} Mbps (configure the shaper separately)"
echo "  Duration:     ${DURATION} seconds"
echo "══════════════════════════════════════════════════════════"
echo ""

# ─── Start iperf3 server in the server namespace ──────────────────────────────
echo "[test] Starting iperf3 server in namespace ${NS_SERVER}..."
ip netns exec "$NS_SERVER" iperf3 --server --port "$IPERF_PORT" --daemon \
    --logfile /tmp/netbucket_iperf3_server.log

echo "[test] iperf3 server started. Waiting 1 second..."
sleep 1

# ─── Run iperf3 client in the client namespace ────────────────────────────────
echo "[test] Running iperf3 client: ${GENERATE_MBPS} Mbps for ${DURATION}s..."
echo ""

ip netns exec "$NS_CLIENT" iperf3 \
    --client "$IP_SERVER" \
    --port "$IPERF_PORT" \
    --bandwidth "${GENERATE_MBPS}M" \
    --time "$DURATION" \
    --interval 1 \
    --udp \
    --verbose

echo ""
echo "[test] Test complete."
echo "[test] Server log: /tmp/netbucket_iperf3_server.log"
echo ""
echo "Expected results:"
echo "  Client sends:    ~${GENERATE_MBPS} Mbps"
echo "  Shaper allows:   ~${RATE_MBPS} Mbps"
echo "  Server receives: ~${RATE_MBPS} Mbps (or less)"
echo ""
echo "Run 'netbucket stats' to see shaper statistics."

#!/usr/bin/env bash


set -euo pipefail

RATE_MBPS=${1:-10}
DURATION=${2:-30}
GENERATE_MBPS=50

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


echo "[test] Starting iperf3 server in namespace ${NS_SERVER}..."
ip netns exec "$NS_SERVER" iperf3 --server --port "$IPERF_PORT" --daemon \
    --logfile /tmp/netbucket_iperf3_server.log

echo "[test] iperf3 server started. Waiting 1 second..."
sleep 1


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

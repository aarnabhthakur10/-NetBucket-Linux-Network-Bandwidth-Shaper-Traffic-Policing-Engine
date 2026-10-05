#!/usr/bin/env bash


set -euo pipefail

NS_CLIENT="netbucket-client"
NS_SERVER="netbucket-server"
IP_SERVER="192.168.100.2"
PORT=5201
DURATION=10
OUTPUT="docs/benchmarks/results.md"

echo "# NetBucket Benchmark Results" > "$OUTPUT"
echo "" >> "$OUTPUT"
echo "Generated: $(date)" >> "$OUTPUT"
echo "" >> "$OUTPUT"

run_test() {
    local label=$1
    local rate_mbps=$2
    local burst_mb=$3
    local mode=$4

    echo "## Test: ${label}" >> "$OUTPUT"
    echo "\`\`\`" >> "$OUTPUT"
    echo "Rate: ${rate_mbps} Mbps | Burst: ${burst_mb} MB | Mode: ${mode}" >> "$OUTPUT"

    ./build/netbucket config \
        --rate "${rate_mbps}Mbps" \
        --burst "${burst_mb}MB" \
        --mode "$mode" 2>/dev/null || true

    ip netns exec "$NS_CLIENT" iperf3 \
        --client "$IP_SERVER" \
        --port "$PORT" \
        --bandwidth "50M" \
        --time "$DURATION" \
        --udp \
        --json >> "$OUTPUT" 2>&1 || echo "iperf3 failed" >> "$OUTPUT"

    echo "\`\`\`" >> "$OUTPUT"
    echo "" >> "$OUTPUT"
}

ip netns exec "$NS_SERVER" iperf3 --server --port "$PORT" --daemon \
    --logfile /tmp/nb_bench_server.log

sleep 1

run_test "10Mbps Shaping  Small Burst" 10  1  shaping
run_test "10Mbps Shaping  Large Burst" 10  20 shaping
run_test "10Mbps Policing Small Burst" 10  1  policing
run_test "20Mbps Shaping"              20  20 shaping
run_test "50Mbps Shaping (no limit)"   50  50 shaping

echo "[benchmark] Results written to $OUTPUT"

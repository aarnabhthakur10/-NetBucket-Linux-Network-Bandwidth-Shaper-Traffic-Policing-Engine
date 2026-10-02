// src/classifier/classifier.hpp
//
// NetBucket — Traffic Classifier
//
// ─── What is traffic classification? ────────────────────────────────────────
//
// Instead of applying a single rate limit to ALL traffic, classification
// splits traffic into named FLOWS, each of which can have its own policy.
//
// Example:
//   HTTP (port 80/443)  → Flow 1 → 5 Mbps bucket
//   Video (port 554)    → Flow 2 → 20 Mbps bucket
//   Bulk  (everything)  → Flow 0 (default) → 2 Mbps bucket
//
// ─── How it works ────────────────────────────────────────────────────────────
//
// A ClassificationRule defines a match condition and assigns a FlowId.
// Rules are evaluated in priority order.
// The first matching rule wins.
// If no rule matches, the packet gets DEFAULT_FLOW_ID (0).
//
// ─── Current match conditions ────────────────────────────────────────────────
//
// Phase 10 supports:
//   - destination port (most common, simplest to implement)
//   - source port
//   - IP protocol (TCP=6, UDP=17)
//   - destination IP (exact match)
//
// Future phases can add:
//   - CIDR prefix matching
//   - Regex on application data
//   - DPI (deep packet inspection)

#pragma once

#include "core/packet.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace netbucket {

// ─── MatchCondition ───────────────────────────────────────────────────────────

enum class MatchField {
    DST_PORT,    ///< Match on TCP/UDP destination port
    SRC_PORT,    ///< Match on TCP/UDP source port
    PROTOCOL,    ///< Match on IP protocol number (6=TCP, 17=UDP, 1=ICMP)
    DST_IP,      ///< Match on destination IPv4 address (exact)
    SRC_IP,      ///< Match on source IPv4 address (exact)
};

struct ClassificationRule {
    std::string  name;       ///< Human-readable name ("http", "video", etc.)
    MatchField   field;      ///< Which header field to match
    uint32_t     value;      ///< Value to match (port number, IP, or protocol)
    FlowId       flow_id;    ///< Assigned flow ID when rule matches
    int          priority;   ///< Lower number = evaluated first

    /// Construct a port-based rule (most common)
    static ClassificationRule by_dst_port(
        std::string name, uint16_t port, FlowId flow_id, int priority = 100);

    static ClassificationRule by_protocol(
        std::string name, uint8_t protocol, FlowId flow_id, int priority = 200);
};

// ─── TrafficClassifier ───────────────────────────────────────────────────────

class TrafficClassifier {
public:
    TrafficClassifier() = default;

    /// Add a classification rule. Rules are sorted by priority automatically.
    void add_rule(ClassificationRule rule);

    /// Remove all rules with the given name.
    void remove_rule(const std::string& name);

    /// Classify a packet: set packet.flow_id based on matching rules.
    /// If no rule matches, packet.flow_id remains DEFAULT_FLOW_ID.
    void classify(Packet& packet) const;

    /// Return the name associated with a FlowId (for logging/display).
    [[nodiscard]] std::string flow_name(FlowId id) const;

    [[nodiscard]] std::size_t rule_count() const { return rules_.size(); }
    [[nodiscard]] const std::vector<ClassificationRule>& rules() const { return rules_; }

private:
    std::vector<ClassificationRule> rules_;   // sorted by priority (ascending)

    [[nodiscard]] bool matches(const ClassificationRule& rule, const Packet& pkt) const;
};

} // namespace netbucket

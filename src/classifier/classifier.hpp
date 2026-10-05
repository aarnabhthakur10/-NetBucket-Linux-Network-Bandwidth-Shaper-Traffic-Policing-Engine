#pragma once

#include "core/packet.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace netbucket {

enum class MatchField {
    DST_PORT,
    SRC_PORT,
    PROTOCOL,
    DST_IP,
    SRC_IP,
};

struct ClassificationRule {
    std::string  name;
    MatchField   field;
    uint32_t     value;
    FlowId       flow_id;
    int          priority;

    static ClassificationRule by_dst_port(
        std::string name, uint16_t port, FlowId flow_id, int priority = 100);

    static ClassificationRule by_protocol(
        std::string name, uint8_t protocol, FlowId flow_id, int priority = 200);
};

class TrafficClassifier {
public:
    TrafficClassifier() = default;

    void add_rule(ClassificationRule rule);

    void remove_rule(const std::string& name);

    void classify(Packet& packet) const;

    [[nodiscard]] std::string flow_name(FlowId id) const;

    [[nodiscard]] std::size_t rule_count() const { return rules_.size(); }
    [[nodiscard]] const std::vector<ClassificationRule>& rules() const { return rules_; }

private:
    std::vector<ClassificationRule> rules_;

    [[nodiscard]] bool matches(const ClassificationRule& rule, const Packet& pkt) const;
};

}

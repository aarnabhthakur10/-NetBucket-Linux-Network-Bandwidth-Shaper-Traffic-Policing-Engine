// src/classifier/classifier.cpp
#include "classifier/classifier.hpp"

#include <algorithm>
#include <stdexcept>

namespace netbucket {

// Rule factories 

ClassificationRule ClassificationRule::by_dst_port(
    std::string name, uint16_t port, FlowId flow_id, int priority)
{
    ClassificationRule r;
    r.name     = std::move(name);
    r.field    = MatchField::DST_PORT;
    r.value    = port;
    r.flow_id  = flow_id;
    r.priority = priority;
    return r;
}

ClassificationRule ClassificationRule::by_protocol(
    std::string name, uint8_t protocol, FlowId flow_id, int priority)
{
    ClassificationRule r;
    r.name     = std::move(name);
    r.field    = MatchField::PROTOCOL;
    r.value    = protocol;
    r.flow_id  = flow_id;
    r.priority = priority;
    return r;
}

// Rule management

void TrafficClassifier::add_rule(ClassificationRule rule) {
    rules_.push_back(std::move(rule));
    // Keep sorted by priority (ascending — lowest priority value evaluated first)
    std::sort(rules_.begin(), rules_.end(),
        [](const ClassificationRule& a, const ClassificationRule& b) {
            return a.priority < b.priority;
        });
}

void TrafficClassifier::remove_rule(const std::string& name) {
    rules_.erase(
        std::remove_if(rules_.begin(), rules_.end(),
            [&name](const ClassificationRule& r) { return r.name == name; }),
        rules_.end()
    );
}

// Classification 

bool TrafficClassifier::matches(const ClassificationRule& rule, const Packet& pkt) const {
    switch (rule.field) {
        case MatchField::DST_PORT:  return pkt.dst_port == static_cast<uint16_t>(rule.value);
        case MatchField::SRC_PORT:  return pkt.src_port == static_cast<uint16_t>(rule.value);
        case MatchField::PROTOCOL:  return pkt.protocol == static_cast<uint8_t>(rule.value);
        case MatchField::DST_IP:    return pkt.dst_ip   == rule.value;
        case MatchField::SRC_IP:    return pkt.src_ip   == rule.value;
    }
    return false;
}

void TrafficClassifier::classify(Packet& packet) const {
    for (const auto& rule : rules_) {
        if (matches(rule, packet)) {
            packet.flow_id = rule.flow_id;
            return;
        }
    }
    // No match — leave as DEFAULT_FLOW_ID (0)
}

std::string TrafficClassifier::flow_name(FlowId id) const {
    if (id == DEFAULT_FLOW_ID) return "default";
    for (const auto& rule : rules_) {
        if (rule.flow_id == id) return rule.name;
    }
    return "flow-" + std::to_string(id);
}

} // namespace netbucket

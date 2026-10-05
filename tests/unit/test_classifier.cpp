#include "classifier/classifier.hpp"
#include "core/packet.hpp"

#include <gtest/gtest.h>

using namespace netbucket;

static Packet make_pkt_with_port(uint16_t dst_port, uint8_t proto = 6) {
    auto p = Packet::make_synthetic(1500);
    p.dst_port = dst_port;
    p.protocol = proto;
    return p;
}

TEST(Classifier, DefaultFlowWhenNoRules) {
    TrafficClassifier c;
    auto pkt = make_pkt_with_port(8080);
    c.classify(pkt);
    EXPECT_EQ(pkt.flow_id, DEFAULT_FLOW_ID);
}

TEST(Classifier, PortMatchAssignsFlow) {
    TrafficClassifier c;
    c.add_rule(ClassificationRule::by_dst_port("http", 80, 1));
    c.add_rule(ClassificationRule::by_dst_port("https", 443, 2));

    auto p80  = make_pkt_with_port(80);
    auto p443 = make_pkt_with_port(443);
    auto pOther = make_pkt_with_port(9999);

    c.classify(p80);
    c.classify(p443);
    c.classify(pOther);

    EXPECT_EQ(p80.flow_id,    1u);
    EXPECT_EQ(p443.flow_id,   2u);
    EXPECT_EQ(pOther.flow_id, DEFAULT_FLOW_ID);
}

TEST(Classifier, PriorityOrder) {
    TrafficClassifier c;
    c.add_rule(ClassificationRule::by_dst_port("low-priority",  80, 99, 100));
    c.add_rule(ClassificationRule::by_dst_port("high-priority", 80, 1,  10));

    auto pkt = make_pkt_with_port(80);
    c.classify(pkt);
    EXPECT_EQ(pkt.flow_id, 1u) << "Higher priority (lower number) rule must win";
}

TEST(Classifier, ProtocolMatch) {
    TrafficClassifier c;
    c.add_rule(ClassificationRule::by_protocol("udp", 17, 10));
    c.add_rule(ClassificationRule::by_protocol("tcp", 6,  20));

    auto udp_pkt = make_pkt_with_port(0, 17);
    auto tcp_pkt = make_pkt_with_port(0, 6);

    c.classify(udp_pkt);
    c.classify(tcp_pkt);

    EXPECT_EQ(udp_pkt.flow_id, 10u);
    EXPECT_EQ(tcp_pkt.flow_id, 20u);
}

TEST(Classifier, RemoveRule) {
    TrafficClassifier c;
    c.add_rule(ClassificationRule::by_dst_port("http", 80, 1));
    EXPECT_EQ(c.rule_count(), 1u);

    c.remove_rule("http");
    EXPECT_EQ(c.rule_count(), 0u);

    auto pkt = make_pkt_with_port(80);
    c.classify(pkt);
    EXPECT_EQ(pkt.flow_id, DEFAULT_FLOW_ID);
}

TEST(Classifier, FlowName) {
    TrafficClassifier c;
    c.add_rule(ClassificationRule::by_dst_port("video", 554, 5));

    EXPECT_EQ(c.flow_name(DEFAULT_FLOW_ID), "default");
    EXPECT_EQ(c.flow_name(5u), "video");
    EXPECT_EQ(c.flow_name(999u), "flow-999");
}

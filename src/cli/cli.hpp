#pragma once

#include "config/config.hpp"

#include <functional>
#include <string>
#include <vector>

namespace netbucket {

class TrafficController;
class StatisticsEngine;

class CLI {
public:
    CLI(ConfigManager& config_mgr,
        TrafficController* controller = nullptr);

    void set_controller(TrafficController* c) { controller_ = c; }

    int run(int argc, char* argv[]);

    int run(const std::vector<std::string>& args);

    static double parse_rate(const std::string& s);

    static double parse_bytes(const std::string& s);

private:
    int cmd_start  (const std::vector<std::string>& args);
    int cmd_stop   ();
    int cmd_status ();
    int cmd_stats  ();
    int cmd_reset  ();
    int cmd_set_rate  (const std::string& value);
    int cmd_set_burst (const std::string& value);
    int cmd_set_mode  (const std::string& mode);
    int cmd_set_queue (const std::string& value);
    int cmd_help   ();

    void print_status() const;

    ConfigManager&    config_mgr_;
    TrafficController* controller_;
};

}

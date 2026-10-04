// src/main.cpp
//
// NetBucket — Daemon Entry Point
//
//  Lifecycle 
//
//   1. Parse CLI arguments
//   2. Load configuration
//   3. Open TUN device (requires CAP_NET_ADMIN / root)
//   4. Create TrafficController with PacketWriter as forward callback
//   5. Create PacketReader and start reading from TUN
//   6. Wait for SIGINT or SIGTERM
//   7. Graceful shutdown: stop reader → stop controller → close TUN
//
//  Signal handling 
//
// SIGINT  (Ctrl+C) and SIGTERM (kill / systemd) both trigger graceful shutdown.
//
// Why handle SIGTERM?
//   When running as a daemon or systemd service, the OS sends SIGTERM to
//   stop the process. Without a handler, the default action is immediate
//   termination — no cleanup, potential resource leaks.
//
// We use a std::atomic<bool> shutdown_requested_ that the signal handler sets.
// The main thread polls this flag in its wait loop.
//
// Why NOT call stop() directly from the signal handler?
//   Signal handlers run asynchronously in a restricted context.
//   Many functions (malloc, printf, std::mutex) are NOT async-signal-safe.
//   Setting an atomic flag is the only safe operation from a signal handler.

#include "cli/cli.hpp"
#include "config/config.hpp"
#include "controller/traffic_controller.hpp"
#include "logger/logger.hpp"
#include "networking/packet_io.hpp"
#include "networking/tun_device.hpp"

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> shutdown_requested{false};

void signal_handler(int sig) {
    (void)sig;
    shutdown_requested.store(true, std::memory_order_relaxed);
}

void install_signals() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

} // anonymous namespace

// Main 

int main(int argc, char* argv[]) {
    install_signals();

    netbucket::ConfigManager config_mgr;

    //  CLI-only mode (no daemon) 
    // If the first argument is a CLI command (not "daemon"), run CLI and exit.
    // This allows:
    //   ./netbucket status
    //   ./netbucket set-rate 20Mbps
    // while the daemon is running in another terminal.

    const std::vector<std::string> daemon_start_cmds = {"daemon", "run", ""};
    std::vector<std::string> cli_args;
    for (int i = 1; i < argc; ++i) cli_args.emplace_back(argv[i]);

    const bool is_daemon_mode =
        cli_args.empty() ||
        cli_args[0] == "daemon" ||
        cli_args[0] == "run";

    if (!is_daemon_mode) {
        // CLI passthrough mode — parse and run a command, then exit
        netbucket::CLI cli{config_mgr, nullptr};
        return cli.run(argc, argv);
    }

    // Daemon mode 

    NB_LOG_INFO("main", "NetBucket v0.1.0 starting");

    // Load config file if specified: ./netbucket daemon --config config/default.json
    for (std::size_t i = 0; i + 1 < cli_args.size(); ++i) {
        if (cli_args[i] == "--config") {
            try {
                config_mgr.load(cli_args[i+1]);
                NB_LOG_INFO("main", "Config loaded from: " + cli_args[i+1]);
            } catch (const std::exception& e) {
                NB_LOG_ERROR("main", std::string("Config load failed: ") + e.what());
                return 1;
            }
        }
    }

    const netbucket::Config cfg = config_mgr.get();

    //  Open TUN device 
    netbucket::TunDevice tun_in{cfg.interface};
    netbucket::TunDevice tun_out{"netbucket_out"};

    bool tun_ok = false;
    try {
        tun_in.open();
        tun_out.open();
        tun_ok = true;
        NB_LOG_INFO("main", "TUN devices opened: " + tun_in.name() + ", " + tun_out.name());
    } catch (const std::exception& e) {
        NB_LOG_WARN("main", std::string("TUN unavailable: ") + e.what());
        NB_LOG_WARN("main", "Running in simulation mode (no real traffic processing)");
    }

    // Build the traffic engine 

    // Forward function: writes allowed packets to the output TUN
    netbucket::ForwardFn forward_fn;
    std::unique_ptr<netbucket::PacketWriter> writer;

    if (tun_ok) {
        writer = std::make_unique<netbucket::PacketWriter>(tun_out);
        forward_fn = writer->as_forward_fn();
    } else {
        // Simulation: forward function just counts the packet
        forward_fn = [](netbucket::Packet) { /* discard in sim mode */ };
    }

    netbucket::TrafficController controller{cfg, std::move(forward_fn)};
    controller.start();

    //  Start packet reader 
    std::unique_ptr<netbucket::PacketReader> reader;
    if (tun_ok) {
        reader = std::make_unique<netbucket::PacketReader>(tun_in, controller);
        reader->start();
    }

    // Status print thread 
    std::thread stats_thread{[&]() {
        while (!shutdown_requested.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds{cfg.stats_interval_ms});

            if (shutdown_requested.load()) break;

            const auto& s = controller.stats();
            NB_LOG_INFO("stats",
                "recv=" + std::to_string(s.packets_received()) +
                " fwd=" + std::to_string(s.packets_forwarded()) +
                " drop=" + std::to_string(s.packets_dropped()) +
                " q=" + std::to_string(s.current_queue_depth()) +
                " tokens=" + std::to_string(
                    static_cast<int>(controller.token_level() / 1024)) + "KB"
            );
        }
    }};

    NB_LOG_INFO("main", "Engine running. Press Ctrl+C to stop.");
    NB_LOG_INFO("main",
        "Rate=" + std::to_string(cfg.rate_bps / 1e6) + " Mbps" +
        " Cap=" + std::to_string(cfg.bucket_capacity_bytes / 1048576.0) + " MB" +
        " Mode=" + netbucket::ConfigManager::mode_to_string(cfg.mode));

    // Wait for shutdown signal 
    while (!shutdown_requested.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    //  Graceful shutdown sequence 
    NB_LOG_INFO("main", "Shutdown requested — stopping gracefully");

    if (reader) reader->stop();
    controller.stop();
    if (tun_ok) {
        tun_in.close();
        tun_out.close();
    }

    if (stats_thread.joinable()) stats_thread.join();

    NB_LOG_INFO("main", "NetBucket stopped cleanly");
    return 0;
}

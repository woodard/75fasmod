#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <getopt.h>
#include <hamlib/rig.h>
#include "DataSocket.hpp"

// Global flag to keep the daemon running
std::atomic<bool> keep_running{true};

void handle_signal(int /* sig */) {
    keep_running = false;
}

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -f, --freq <MHz>      Frequency to set in MHz (e.g., 144.390)\n"
              << "  -p, --port <device>   Serial port (default: /dev/ttyUSB0)\n"
              << "  -m, --model <id>      Hamlib rig model ID (default: 2 for generic Kenwood)\n"
              << "  -s, --sock <path>     Data socket path (default: /tmp/75fasmod_data.sock)\n"
              << "  -h, --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    double target_freq_mhz = 0.0;
    freq_t target_freq_hz = 0;
    std::string serial_port = "/dev/ttyUSB0";
    std::string sock_path = "/tmp/75fasmod_data.sock";
    rig_model_t rig_model = RIG_MODEL_KENWOOD; 

    const char* const short_opts = "f:p:m:s:h";
    const option long_opts[] = {
        {"freq", required_argument, nullptr, 'f'},
        {"port", required_argument, nullptr, 'p'},
        {"model", required_argument, nullptr, 'm'},
        {"sock", required_argument, nullptr, 's'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'f':
                target_freq_mhz = std::stod(optarg);
                target_freq_hz = static_cast<freq_t>(target_freq_mhz * 1000000.0);
                break;
            case 'p': serial_port = optarg; break;
            case 'm': rig_model = std::stoi(optarg); break;
            case 's': sock_path = optarg; break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (target_freq_hz == 0) {
        std::cerr << "Error: You must specify a target frequency in MHz.\n";
        return 1;
    }

    // 1. Setup Signal Handler for graceful shutdown
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    // 2. Initialize Hamlib and set frequency
    std::cout << "Initializing Hamlib (Model ID: " << rig_model << ")...\n";
    RIG* my_rig = rig_init(rig_model);
    if (!my_rig) {
        std::cerr << "Error: Hamlib initialization failed.\n";
        return 1;
    }

    strncpy(my_rig->state.rigport.pathname, serial_port.c_str(), FILPATHLEN - 1);
    
    if (rig_open(my_rig) == RIG_OK) {
        std::cout << "Setting frequency to " << target_freq_mhz << " MHz...\n";
        rig_set_freq(my_rig, RIG_VFO_CURR, target_freq_hz);
    } else {
        std::cerr << "Warning: Could not open radio on " << serial_port << ". Continuing for testing...\n";
    }

    // 3. Start the Data Socket Server
    DataSocket data_sock(sock_path);
    if (!data_sock.start()) {
        rig_close(my_rig);
        rig_cleanup(my_rig);
        return 1;
    }

    // 4. Main Daemon Loop
    std::cout << "Daemon is running. Press Ctrl+C to stop.\n";
    while (keep_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // 5. Cleanup
    std::cout << "\nShutting down daemon...\n";
    data_sock.stop(); // Stops the jthread safely
    rig_close(my_rig);
    rig_cleanup(my_rig);

    return 0;
}
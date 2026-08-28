#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <getopt.h>
#include <hamlib/rig.h>
#include "DataSocket.hpp"
#include "RadioController.hpp"
#include "ModemDSP.hpp"

std::atomic<bool> keep_running{true};

void handle_signal(int /* sig */) {
    keep_running = false;
}

void print_usage(const char* prog_name) {
  std::cout << "Usage: " << prog_name << " [options]\n"
            << "Options:\n"
            << "  -f, --freq <MHz>      Frequency to set in MHz (e.g., 144.390)\n"
            << "  -w, --power <level>   TX Power level (EL, L, M, H)\n"
            << "  -p, --port <device>   Serial port (default: /dev/ttyUSB0)\n"
            << "  -m, --model <id>      Hamlib rig model ID (default: 2 for "
               "generic Kenwood)\n"
            << "  -s, --sock <path>     Data socket path (default: "
               "/tmp/75fasmod_data.sock)\n"
            << "  -b, --burst <count>   Max frames per TX burst (default: 8)\n"
            << "  -t, --timeout <ms>    TX queue flush timeout in ms (default: "
               "200)\n"
            << "  -h, --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    double target_freq_mhz = 0.0;
    std::string power_level = ""; // Empty string means leave power as-is
    std::string serial_port = "/dev/ttyUSB0";
    std::string sock_path = "/tmp/75fasmod_data.sock";
    rig_model_t rig_model = RIG_MODEL_KENWOOD; 

    const char* const short_opts = "f:w:p:m:s:h";
    const option long_opts[] = {
        {"freq", required_argument, nullptr, 'f'},
        {"power", required_argument, nullptr, 'w'},
        {"port", required_argument, nullptr, 'p'},
        {"model", required_argument, nullptr, 'm'},
        {"sock", required_argument, nullptr, 's'},
        {"burst", required_argument, nullptr, 'b'},
        {"timeout", required_argument, nullptr, 't'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'f': target_freq_mhz = std::stod(optarg); break;
            case 'w': power_level = optarg; break;
            case 'p': serial_port = optarg; break;
            case 'm': rig_model = std::stoi(optarg); break;
            case 's': sock_path = optarg; break;
            case 'b': burst_limit = std::stoi(optarg); break;
            case 't': flush_timeout_ms = std::stoi(optarg); break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    if (target_freq_mhz == 0.0) {
        std::cerr << "Error: You must specify a target frequency in MHz.\n";
        return 1;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    RadioController radio(rig_model, serial_port);
    if (!radio.initialize()) {
        std::cerr << "Warning: Radio init failed. Proceeding without rig control.\n";
    } else {
        std::cout << "Setting frequency to " << target_freq_mhz << " MHz...\n";
        radio.set_frequency(target_freq_mhz);

        // Set power if specified by the user
        if (!power_level.empty()) {
            radio.set_power_level(power_level);
        }
    }

    ModemDSP dsp;
    DataSocket data_sock(sock_path, radio, dsp, burst_limit, flush_timeout_ms);
    if (!data_sock.start()) {
        return 1;
    }

    std::cout << "Daemon is running. Press Ctrl+C to stop.\n";
    while (keep_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\nShutting down daemon...\n";
    data_sock.stop();

    // The RadioController destructor will automatically run here
    // and restore the original power level, mode, and menu settings.
    return 0;
}

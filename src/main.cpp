#include "DataSocket.hpp"
#include "ModemDSP.hpp"
#include "RadioController.hpp"
#include "THD75.hpp"
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <getopt.h>
#include <hamlib/rig.h>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Global flag to keep the daemon running
std::atomic<bool> keep_running{true};

void handle_signal(int /* sig */) { keep_running = false; }

void print_usage(const char *prog_name) {
  std::cout
      << "Usage: " << prog_name << " [options]\n"
      << "Options:\n"
      << "  -f, --freq <MHz>      Frequency to set in MHz (e.g., 144.390)\n"
      << "  -w, --power <level>   TX Power level (EL, L, M, H)\n"
      << "  -p, --port <device>   Serial port (default: auto-discover Kenwood "
         "TH-D75)\n"
      << "  -m, --model <id>      Hamlib rig model ID (default: 2 for generic "
         "Kenwood)\n"
      << "  -s, --sock <path>     Data socket path (default: "
         "/tmp/75fasmod_data.sock)\n"
      << "  -b, --burst <count>   Max frames per TX burst (default: 8)\n"
      << "  -t, --timeout <ms>    TX queue flush timeout in ms (default: 200)\n"
      << "  -h, --help            Show this help message\n"
      << "  -a, --alsa-tx <device>  ALSA transmit device (e.g., hw:5,0)\n"
      << "  -d, --hamlib-debug   Enable Hamlib debug logging\n";
}

int main(int argc, char *argv[]) {
  // Variable Declarations (Correctly scoped for the entire main function)
  double target_freq_mhz = 0.0;
  std::string power_level = "";
  std::string serial_port = "";
  std::string sock_path = "/tmp/75fasmod_data.sock";
  std::string alsa_tx_device = "";
  bool hamlib_debug = false;

  // Default values
  rig_model_t rig_model = THD75::DEFAULT_MODEL;
  int burst_limit = 8;
  int flush_timeout_ms = 200;

  const char *const short_opts = "f:w:p:m:s:b:t:h:a:d";
  const option long_opts[] = {{"freq", required_argument, nullptr, 'f'},
                              {"power", required_argument, nullptr, 'w'},
                              {"port", required_argument, nullptr, 'p'},
                              {"model", required_argument, nullptr, 'm'},
                              {"sock", required_argument, nullptr, 's'},
                              {"burst", required_argument, nullptr, 'b'},
                              {"timeout", required_argument, nullptr, 't'},
                              {"help", no_argument, nullptr, 'h'},
                              {"alsa-tx", required_argument, nullptr, 'a'},
                              {"hamlib-debug", no_argument, nullptr, 'd'},
                              {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) !=
         -1) {
    switch (opt) {
    case 'f':
      target_freq_mhz = std::stod(optarg);
      break;
    case 'w':
      power_level = optarg;
      break;
    case 'p':
      serial_port = optarg;
      break;
    case 'm':
      rig_model = std::stoi(optarg);
      break;
    case 's':
      sock_path = optarg;
      break;
    case 'b':
      burst_limit = std::stoi(optarg);
      break;
    case 't':
      flush_timeout_ms = std::stoi(optarg);
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    case 'a':
      alsa_tx_device = optarg;
      break;
    case 'd':
      hamlib_debug = true;
      break;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  if (target_freq_mhz == 0.0) {
    std::cerr << "Error: You must specify a target frequency in MHz.\n";
    return 1;
  }

  if (alsa_tx_device.empty()) {
    std::cerr << "Error: You must specify an ALSA transmit device.\n";
    return 1;
  }

  // 1. Setup Signal Handler
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  // 2. Discover Kenwood TH-D75 device if not explicitly specified
  if (serial_port.empty()) {
    std::vector<std::string> discovered_ports =
        RadioController::find_tty_sysfs();

    if (discovered_ports.size() == 1) {
      serial_port = discovered_ports[0];
      std::cout << "Auto-discovered Kenwood TH-D75 at " << serial_port << "\n";
    } else if (discovered_ports.size() > 1) {
      std::cout << "Multiple Kenwood TH-D75 devices found:\n";
      for (size_t i = 0; i < discovered_ports.size(); ++i) {
        std::cout << "  [" << i << "] " << discovered_ports[i] << "\n";
      }
      std::cout << "Select device (0-" << (discovered_ports.size() - 1)
                << "): ";
      size_t choice;
      std::cin >> choice;
      if (choice < discovered_ports.size()) {
        serial_port = discovered_ports[choice];
      } else {
        std::cerr << "Invalid selection\n";
        return 1;
      }
    } else {
      // Fall back to common default if discovery fails
      serial_port = "/dev/ttyUSB0";
      std::cout
          << "Warning: Could not auto-discover Kenwood TH-D75, using default "
          << serial_port << "\n";
    }
  }

  // 3. Initialize Hardware & DSP Classes
  THD75 radio(serial_port, rig_model, hamlib_debug);
  if (!radio.initialize()) {
    std::cerr << "Error: Failed to initialize radio controller.\n";
    return 1;
  } else {
    std::cout << "Setting frequency to " << target_freq_mhz << " MHz...\n";
    radio.set_frequency(target_freq_mhz);

    if (!power_level.empty()) {
      radio.set_power_level(power_level);
    }
  }

  // Map the serial port to its matching ALSA soundcard
  std::string alsa_device = radio.find_alsa_device();
  if (alsa_device.empty()) {
    std::cerr << "Warning: Could not map serial port to ALSA device. "
                 "Defaulting to TH-D75 alias.\n";
    alsa_device = "hw:CARD=THD75,DEV=0";
  } else {
    std::cout << "Mapped serial port " << serial_port
              << " to ALSA audio device " << alsa_device << "\n";
  }

  ModemDSP dsp(alsa_tx_device, alsa_device);

  // 4. Start the Data Socket Server
  DataSocket data_sock(sock_path, radio, dsp, burst_limit, flush_timeout_ms);
  if (!data_sock.start()) {
    return 1;
  }

  // 5. Main Daemon Loop
  std::cout << "Daemon is running. Press Ctrl+C to stop.\n";
  while (keep_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  // 6. Cleanup
  std::cout << "\nShutting down daemon...\n";
  data_sock.stop();

  return 0;
}

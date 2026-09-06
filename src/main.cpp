#include "DataSocket.hpp"
#include "ModemDSP.hpp"
#include "RadioController.hpp"
#include "THD75.hpp"
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <boost/program_options.hpp>
#include <hamlib/rig.h>
#include <string>
#include <vector>

namespace po = boost::program_options;

namespace {
constexpr int BURST_LIMIT = 8;
constexpr int FLUSH_TIMEOUT_MS = 200;
constexpr int SLEEP_DURATION_MS = 200;
} // namespace

namespace fs = std::filesystem;

// Global flag to keep the daemon running
std::atomic<bool> keep_running{true};

void handle_signal(int /* sig */) { keep_running = false; }

void print_usage(const char *prog_name, po::options_description &desc) {
  std::cout
      << "Usage: " << prog_name << " [options]\n"
      << desc << "\n";
}

auto main(int argc, char *argv[]) -> int {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "Show this help message")
    ("freq,f", po::value<double>()->default_value(0.0),
     "Frequency to set in MHz (e.g., 144.390)")
    ("power,w", po::value<std::string>(),
     "TX Power level (EL, L, M, H)")
    ("port,p", po::value<std::string>()->default_value(""),
     "Serial port (default: auto-discover Kenwood TH-D75)")
    ("model,m", po::value<rig_model_t>()->default_value(THD75::DEFAULT_MODEL),
     "Hamlib rig model ID (default: 2 for generic Kenwood)")
    ("sock,s", po::value<std::string>()->default_value("/tmp/75fasmod_data.sock"),
     "Data socket path (default: /tmp/75fasmod_data.sock)")
    ("burst,b", po::value<int>()->default_value(BURST_LIMIT),
     "Max frames per TX burst (default: 8)")
    ("timeout,t", po::value<int>()->default_value(FLUSH_TIMEOUT_MS),
     "TX queue flush timeout in ms (default: 200)")
    ("alsa-tx,a", po::value<std::string>(),
     "ALSA transmit device (e.g., hw:5,0)")
    ("hamlib-debug,d", po::bool_switch()->default_value(false),
     "Enable Hamlib debug logging");

  // Parse the command line
  po::variables_map options_map;
  try {
    po::store(po::parse_command_line(argc, argv, desc), options_map);
    po::notify(options_map);
  } catch (const po::error &e) {
    std::cerr << "Error: " << e.what() << "\n";
    print_usage(argv[0], desc);
    return 1;
  }

  // Handle help
  // NOLINTNEXTLINE(readability-container-contains)
  if (options_map.count("help") > 0) {
    print_usage(argv[0], desc);
    return 0;
  }

  // Variable Declarations
  double target_freq_mhz = options_map["freq"].as<double>();
  std::string power_level = options_map["power"].as<std::string>();
  std::string serial_port = options_map["port"].as<std::string>();
  std::string sock_path = options_map["sock"].as<std::string>();
  std::string alsa_tx_device = options_map["alsa-tx"].as<std::string>();
  bool hamlib_debug = options_map["hamlib-debug"].as<bool>();

  // Defaults
  rig_model_t rig_model = options_map["model"].as<rig_model_t>();
  int burst_limit = options_map["burst"].as<int>();
  int flush_timeout_ms = options_map["timeout"].as<int>();

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
  }
  {
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
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_DURATION_MS));
  }

  // 6. Cleanup
  std::cout << "\nShutting down daemon...\n";
  data_sock.stop();

  return 0;
}

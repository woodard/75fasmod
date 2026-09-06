/**
 * @file test_other_frequency.cpp
 * @brief Test program for THD75 other frequency get/set functionality
 *
 * Tests the set_other_frequency and get_other_frequency functions which
 * operate on the VFO that is NOT currently transmitting.
 */

#include "THD75.hpp"
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;

static double get_different_freq_in_band(double current_freq_mhz) {
  if (current_freq_mhz >= 144.0 && current_freq_mhz <= 148.0) {
    return (current_freq_mhz >= 146.0) ? 144.500 : 146.500;
  } else if (current_freq_mhz >= 222.0 && current_freq_mhz <= 225.0) {
    return (current_freq_mhz >= 223.5) ? 222.500 : 224.500;
  } else if (current_freq_mhz >= 420.0 && current_freq_mhz <= 450.0) {
    return (current_freq_mhz >= 435.0) ? 430.000 : 442.000;
  }
  return current_freq_mhz + 0.100;
}

int main(int argc, char *argv[]) {
  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "Show this help message")(
      "set,s", po::value<std::string>()->implicit_value(""),
      "Set frequency to specified MHz or a different one in band");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (const po::error &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  if (vm.count("help")) {
    std::cout << "Usage: " << argv[0] << " [-s [freq_mhz]]\n";
    return 0;
  }

  bool set_flag = vm.count("set");
  std::string freq_str = vm["set"].as<std::string>();
  double explicit_freq = 0.0;

  if (set_flag && !freq_str.empty()) {
    explicit_freq = std::stod(freq_str);
  }

  std::vector<std::string> ports = RadioController::find_tty_sysfs();
  if (ports.empty()) {
    std::cerr << "[ERROR] No radio serial ports found via sysfs.\n";
    return 1;
  }

  THD75 radio(ports[0], THD75::DEFAULT_MODEL, true);

  // Put radio in dual mode for other VFO operations
  if (radio.get_single()) {
    std::cerr << "[INFO] Radio in single mode - enabling dual mode\n";
    if (!radio.set_dual()) {
      std::cerr << "[ERROR] Failed to enable dual mode\n";
      return 1;
    }
    std::cerr << "[INFO] Dual mode enabled\n";
  }

  if (set_flag) {
    double target_freq =
        (explicit_freq > 0.0) ? explicit_freq : get_different_freq_in_band(0.0);
    if (!radio.set_other_frequency(target_freq)) {
      std::cerr << "[ERROR] Failed to set other frequency to " << target_freq
                << " MHz\n";
      return 1;
    }
    std::cout << target_freq << "\n";
  } else {
    double freq = radio.get_other_frequency();
    std::cout << freq << "\n";
  }

  return 0;
}

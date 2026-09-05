/**
 * @file test_frequency.cpp
 * @brief Test program for RadioController frequency get/set functionality
 */

#include "THD75.hpp"
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

// Calculates a distinct valid frequency in the same amateur band
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
  bool set_flag = false;
  double explicit_freq = 0.0;

  const char *const short_opts = "s::h";
  const option long_opts[] = {{"set", optional_argument, nullptr, 's'},
                              {"help", no_argument, nullptr, 'h'},
                              {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) !=
         -1) {
    switch (opt) {
    case 's':
      set_flag = true;
      if (optarg) {
        explicit_freq = std::stod(optarg);
      }
      break;
    case 'h':
      std::cout << "Usage: " << argv[0] << " [-s [freq_mhz]]\n";
      return 0;
    default:
      return 1;
    }
  }

  std::vector<std::string> ports = RadioController::find_tty_sysfs();
  if (ports.empty()) {
    std::cerr << "[ERROR] No radio serial ports found via sysfs.\n";
    return 1;
  }

  THD75 radio(ports[0]);
  if (!radio.initialize()) {
    std::cerr << "[ERROR] Failed to initialize RadioController on " << ports[0]
              << "\n";
    return 1;
  }

  double current_freq_mhz = 0.0;
  if (!radio.get_frequency(current_freq_mhz)) {
    std::cerr << "[ERROR] Failed to query current frequency.\n";
    return 1;
  }

  if (set_flag) {
    double target_freq = (explicit_freq > 0.0)
                             ? explicit_freq
                             : get_different_freq_in_band(current_freq_mhz);
    if (!radio.set_frequency(target_freq)) {
      std::cerr << "[ERROR] Failed to set frequency to " << target_freq
                << " MHz\n";
      return 1;
    }
    std::cout << target_freq << "\n";
  } else {
    std::cout << current_freq_mhz << "\n";
  }

  return 0;
}
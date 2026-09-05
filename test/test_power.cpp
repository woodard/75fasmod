/**
 * @file test_power.cpp
 * @brief Test program for RadioController power level get/set functionality
 */

#include "THD75.hpp"
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

// Returns a valid power level different from the current one
static std::string get_different_power_level(const std::string &current_level) {
  if (current_level == "H") {
    return "M";
  } else if (current_level == "M") {
    return "L";
  } else if (current_level == "L") {
    return "EL";
  } else if (current_level == "EL") {
    return "H";
  }
  return "M";
}

int main(int argc, char *argv[]) {
  bool set_flag = false;
  std::string explicit_level;

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
        explicit_level = optarg;
      }
      break;
    case 'h':
      std::cout << "Usage: " << argv[0] << " [-s [level]]\n";
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

  THD75 radio(RadioController::DEFAULT_MODEL, ports[0]);
  if (!radio.initialize()) {
    std::cerr << "[ERROR] Failed to initialize RadioController on " << ports[0]
              << "\n";
    return 1;
  }

  std::string current_level;
  if (!radio.get_power_level(current_level)) {
    std::cerr << "[ERROR] Failed to query current power level.\n";
    return 1;
  }

  if (set_flag) {
    std::string target_level = (!explicit_level.empty())
                                   ? explicit_level
                                   : get_different_power_level(current_level);
    if (!radio.set_power_level(target_level)) {
      std::cerr << "[ERROR] Failed to set power level to " << target_level
                << "\n";
      return 1;
    }
    std::cout << target_level << "\n";
  } else {
    std::cout << current_level << "\n";
  }

  return 0;
}
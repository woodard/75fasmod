/**
 * @file test_mode.cpp
 * @brief Test program for RadioController mode get/set functionality
 */

#include "RadioController.hpp"
#include "THD75.hpp"
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

// Helper function to convert Mode enum to string
static std::string mode_to_string(RadioController::Mode mode) {
  switch (mode) {
  case RadioController::Mode::FM: return "FM";
  case RadioController::Mode::WFM: return "WFM";
  case RadioController::Mode::AM: return "AM";
  case RadioController::Mode::USB: return "USB";
  case RadioController::Mode::LSB: return "LSB";
  case RadioController::Mode::CW: return "CW";
  case RadioController::Mode::CWR: return "CWR";
  case RadioController::Mode::DD: return "DD";
  default: return "UNKNOWN";
  }
}

// Helper function to convert string to Mode enum
static RadioController::Mode string_to_mode(const std::string &str) {
  if (str == "FM") return RadioController::Mode::FM;
  if (str == "WFM") return RadioController::Mode::WFM;
  if (str == "AM") return RadioController::Mode::AM;
  if (str == "USB") return RadioController::Mode::USB;
  if (str == "LSB") return RadioController::Mode::LSB;
  if (str == "CW") return RadioController::Mode::CW;
  if (str == "CWR") return RadioController::Mode::CWR;
  if (str == "DD") return RadioController::Mode::DD;
  return RadioController::Mode::FM; // Default
}

int main(int argc, char *argv[]) {
  bool set_flag = false;
  std::string explicit_mode;

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
        explicit_mode = optarg;
      } else {
        // Default to USB if no mode specified
        explicit_mode = "USB";
      }
      break;
    case 'h':
      std::cout << "Usage: " << argv[0] << " [-s [mode]]\n";
      std::cout << "Modes: FM, WFM, AM, USB, LSB, CW, CWR, DD\n";
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

  THD75 radio(ports[0], THD75::DEFAULT_MODEL, true);

  RadioController::Mode current_mode;
  if (!radio.get_mode(current_mode)) {
    std::cerr << "[ERROR] Failed to query current mode.\n";
    return 1;
  }

  if (set_flag) {
    RadioController::Mode target_mode = string_to_mode(explicit_mode);
    if (!radio.set_mode(target_mode)) {
      std::cerr << "[ERROR] Failed to set mode to " << explicit_mode << "\n";
      return 1;
    }
    std::cout << mode_to_string(target_mode) << "\n";
  } else {
    std::cout << mode_to_string(current_mode) << "\n";
  }

  return 0;
}
#ifdef DEBUG_MODE
#include <iostream>

namespace {
void debug_test() {
  std::vector<std::string> ports = RadioController::find_tty_sysfs();
  if (ports.empty()) {
    std::cerr << "No ports found" << std::endl;
    return;
  }
  
  THD75 radio(ports[0], THD75::DEFAULT_MODEL, true);
  
  RadioController::Mode mode;
  std::cerr << "Testing get_mode..." << std::endl;
  bool result = radio.get_mode(mode);
  std::cerr << "get_mode returned: " << result << std::endl;
  if (result) {
    std::cerr << "Mode: " << mode_to_string(mode) << std::endl;
  }
}
}
#endif

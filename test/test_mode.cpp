/**
 * @file test_mode.cpp
 * @brief Test program for RadioController mode get/set functionality
 */

#include "RadioController.hpp"
#include "THD75.hpp"
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;

// Helper function to convert Mode enum to string
static std::string mode_to_string(RadioController::Mode mode) {
  switch (mode) {
  case RadioController::Mode::FM:
    return "FM";
  case RadioController::Mode::WFM:
    return "WFM";
  case RadioController::Mode::AM:
    return "AM";
  case RadioController::Mode::USB:
    return "USB";
  case RadioController::Mode::LSB:
    return "LSB";
  case RadioController::Mode::CW:
    return "CW";
  case RadioController::Mode::CWR:
    return "CWR";
  case RadioController::Mode::DD:
    return "DD";
  default:
    return "UNKNOWN";
  }
}

// Helper function to convert string to Mode enum
static RadioController::Mode string_to_mode(const std::string &str) {
  if (str == "FM")
    return RadioController::Mode::FM;
  if (str == "WFM")
    return RadioController::Mode::WFM;
  if (str == "AM")
    return RadioController::Mode::AM;
  if (str == "USB")
    return RadioController::Mode::USB;
  if (str == "LSB")
    return RadioController::Mode::LSB;
  if (str == "CW")
    return RadioController::Mode::CW;
  if (str == "CWR")
    return RadioController::Mode::CWR;
  if (str == "DD")
    return RadioController::Mode::DD;
  return RadioController::Mode::FM; // Default
}

int main(int argc, char *argv[]) {
  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "Show this help message")(
      "set,s", po::value<std::string>()->implicit_value("USB"),
      "Set mode to specified value or default to USB");

  // Parse the command line
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (const po::error &e) {
    std::cerr << "Error: " << e.what() << "\n";
    std::cout << "Usage: " << argv[0] << " [-s [mode]]\n";
    return 1;
  }

  // Handle help
  if (vm.count("help")) {
    std::cout << "Usage: " << argv[0] << " [-s [mode]]\n";
    std::cout << "Modes: FM, WFM, AM, USB, LSB, CW, CWR, DD\n";
    return 0;
  }

  bool set_flag = vm.count("set");
  std::string explicit_mode = vm["set"].as<std::string>();

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

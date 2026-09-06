/**
 * @file test_power.cpp
 * @brief Test program for RadioController power level get/set functionality
 */

#include "THD75.hpp"
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;

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
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "Show this help message")
    ("set,s", po::value<std::string>()->implicit_value(""),
     "Set power level to specified value or a different one");

  // Parse the command line
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (const po::error &e) {
    std::cerr << "Error: " << e.what() << "\n";
    std::cout << "Usage: " << argv[0] << " [-s [level]]\n";
    return 1;
  }

  // Handle help
  if (vm.count("help")) {
    std::cout << "Usage: " << argv[0] << " [-s [level]]\n";
    return 0;
  }

  bool set_flag = vm.count("set");
  std::string explicit_level;
  
  if (set_flag && vm["set"].as<std::string>().empty() == false) {
    explicit_level = vm["set"].as<std::string>();
  }

  std::vector<std::string> ports = RadioController::find_tty_sysfs();
  if (ports.empty()) {
    std::cerr << "[ERROR] No radio serial ports found via sysfs.\n";
    return 1;
  }

  THD75 radio(ports[0], THD75::DEFAULT_MODEL, true);
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

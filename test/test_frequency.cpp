/**
 * @file test_frequency.cpp
 * @brief Test program for RadioController frequency get/set functionality
 *
 * This test program:
 * 1. Uses find_tty_sysfs to identify the radio's serial port
 * 2. Instantiates a RadioController with that serial port
 * 3. Calls get_frequency() to find out the current frequency
 * 4. If -s option is passed, sets the frequency to something different within the same ham band
 * 5. Prints out the frequency that it set or got
 */

#include <getopt.h>
#include <hamlib/rig.h>
#include <iostream>
#include <string>
#include <vector>
#include "RadioController.hpp"

void print_usage(const char *prog_name) {
  std::cout << "Usage: " << prog_name << " [options]\n"
            << "Options:\n"
            << "  -s, --set-freq <MHz>  Set frequency to specified value (MHz)\n"
            << "  -h, --help            Show this help message\n"
            << "  -n, --new-freq <MHz>  New frequency to try (for comparison test)\n";
}

int main(int argc, char *argv[]) {
  double new_freq_mhz = 0.0;
  double compare_freq_mhz = 435.0; // Default comparison frequency
  bool do_set = false;

  const char *const short_opts = "s:h:n:";
  const option long_opts[] = {
      {"set-freq", required_argument, nullptr, 's'},
      {"help", no_argument, nullptr, 'h'},
      {"new-freq", required_argument, nullptr, 'n'},
      {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
    switch (opt) {
    case 's':
      new_freq_mhz = std::stod(optarg);
      do_set = true;
      break;
    case 'n':
      compare_freq_mhz = std::stod(optarg);
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  // Step 1: Find the radio's serial port using find_tty_sysfs
  std::cout << "[TEST] Step 1: Searching for radio serial port via sysfs..." << std::endl;
  std::vector<std::string> ports = RadioController::find_tty_sysfs();

  if (ports.empty()) {
    std::cerr << "[ERROR] No radio serial ports found! Check your USB connection." << std::endl;
    return 1;
  }

  std::string radio_port = ports[0];
  std::cout << "[SUCCESS] Found radio on: " << radio_port << std::endl;

  // Step 2: Instantiate RadioController with that serial port
  std::cout << "\n[TEST] Step 2: Instantiating RadioController..." << std::endl;
  RadioController radio(RadioController::DEFAULT_MODEL, radio_port);

  if (!radio.initialize()) {
    std::cerr << "[ERROR] Failed to initialize RadioController on " << radio_port << std::endl;
    return 1;
  }
  std::cout << "[SUCCESS] RadioController initialized." << std::endl;

  // Step 3: Get current frequency
  std::cout << "\n[TEST] Step 3: Getting current frequency..." << std::endl;
  double current_freq_mhz = 0.0;
  if (!radio.get_frequency(current_freq_mhz)) {
    std::cerr << "[ERROR] Failed to get frequency from radio." << std::endl;
    return 1;
  }
  std::cout << "[INFO] Current frequency: " << current_freq_mhz << " MHz" << std::endl;

  if (do_set) {
    // Step 4: Set frequency to a new value
    std::cout << "\n[TEST] Step 4: Setting frequency to " << new_freq_mhz << " MHz..." << std::endl;
    if (!radio.set_frequency(new_freq_mhz)) {
      std::cerr << "[ERROR] Failed to set frequency to " << new_freq_mhz << " MHz" << std::endl;
      return 1;
    }
    std::cout << "[SUCCESS] Successfully set frequency to " << new_freq_mhz << " MHz" << std::endl;
    std::cout << "\n[OUTPUT] " << new_freq_mhz << std::endl;
  } else {
    // Just print the current frequency
    std::cout << "\n[INFO] No -s option provided, just reporting current frequency." << std::endl;
    std::cout << "\n[OUTPUT] " << current_freq_mhz << std::endl;
  }

  return 0;
}
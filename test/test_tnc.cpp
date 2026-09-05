/**
 * @file test_tnc.cpp
 * @brief Test program for THD75 TNC get/set functionality
 */

#include "THD75.hpp"
#include <getopt.h>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  const char *const short_opts = "h";
  const option long_opts[] = {{"help", no_argument, nullptr, 'h'},
                              {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) !=
         -1) {
    switch (opt) {
    case 'h':
      std::cout << "Usage: " << argv[0] << "\n"
                << "Tests THD75 get_tnc/set_tnc functions.\n";
      return 0;
    default:
      return 1;
    }
  }

  // Find radio port
  std::vector<std::string> ports = RadioController::find_tty_sysfs();
  if (ports.empty()) {
    std::cerr << "[ERROR] No radio serial ports found.\n";
    return 1;
  }

  THD75 radio(ports[0]);
  if (!radio.initialize()) {
    std::cerr << "[ERROR] Failed to initialize radio.\n";
    return 1;
  }

  bool all_passed = true;

  std::cout << "=== Testing get_tnc ===\n";
  int original_tnc = radio.get_tnc();
  std::cout << "Original TNC mode: " << original_tnc << "\n";
  std::cout << "[PASS] get_tnc succeeded\n";

  std::cout << "\n=== Testing set_tnc ===\n";
  if (radio.set_tnc(0)) {
    std::cout << "[PASS] set_tnc(0) succeeded\n";
    int verify = radio.get_tnc();
    std::cout << "TNC read as: " << verify << "\n";
  } else {
    std::cout << "[FAIL] set_tnc(0) failed\n";
    all_passed = false;
  }

  // Restore original TNC
  if (original_tnc >= 0 && original_tnc != 0) {
    radio.set_tnc(original_tnc);
    std::cout << "Restored TNC to " << original_tnc << "\n";
  }

  radio.shutdown();

  return all_passed ? 0 : 1;
}

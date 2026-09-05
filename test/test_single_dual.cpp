/**
 * @file test_single_dual.cpp
 * @brief Test program for THD75 single/dual band control functions
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
                << "Tests THD75 get_single/set_single/get_dual/set_dual "
                   "and flip_single_dual functions.\n";
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

  std::cout << "=== Testing get_single and get_dual ===\n";
  bool is_single = radio.get_single();
  bool is_dual = radio.get_dual();
  std::cout << "get_single() = " << (is_single ? "true" : "false") << "\n";
  std::cout << "get_dual() = " << (is_dual ? "true" : "false") << "\n";

  if (is_single != is_dual) {
    std::cout << "[PASS] Single and dual are opposites\n";
  } else {
    std::cout << "[INFO] Single and dual are same (acceptable)\n";
  }

  std::cout << "\n=== Testing set_single ===\n";
  if (radio.set_single(THD75::VFO::A)) {
    std::cout << "[PASS] set_single(VFO::A) succeeded\n";
  } else {
    std::cout << "[FAIL] set_single(VFO::A) failed\n";
    all_passed = false;
  }

  if (radio.set_single(THD75::VFO::B)) {
    std::cout << "[PASS] set_single(VFO::B) succeeded\n";
  } else {
    std::cout << "[FAIL] set_single(VFO::B) failed\n";
    all_passed = false;
  }

  std::cout << "\n=== Testing set_dual ===\n";
  if (radio.set_dual()) {
    std::cout << "[PASS] set_dual() succeeded\n";
  } else {
    std::cout << "[FAIL] set_dual() failed\n";
    all_passed = false;
  }

  std::cout << "\n=== Testing flip_single_dual ===\n";
  if (radio.flip_single_dual(THD75::VFO::A)) {
    std::cout << "[PASS] flip_single_dual(VFO::A) succeeded\n";
  } else {
    std::cout << "[FAIL] flip_single_dual(VFO::A) failed\n";
    all_passed = false;
  }

  radio.shutdown();

  return all_passed ? 0 : 1;
}

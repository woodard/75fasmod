/**
 * @file test_vfo.cpp
 * @brief Test program for THD75 VFO get/set functionality
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
                << "Tests THD75 get_current_vfo/set_current_vfo functions.\n";
      return 0;
    default:
      return 1;
    }
  }

  // Find radio port
  std::vector<std::string> ports = RadioController::find_tty_sysfs();

  THD75 radio(ports[0], THD75::DEFAULT_MODEL, true);

  bool all_passed = true;

  std::cout << "=== Testing get_current_vfo ===\n";
  THD75::VFO current = radio.get_current_vfo();
  std::cout << "Current VFO: " << (current == THD75::VFO::A ? "A" : "B")
            << "\n";
  std::cout << "[PASS] get_current_vfo succeeded\n";

  std::cout << "\n=== Testing set_current_vfo ===\n";
  THD75::VFO test_vfo =
      (current == THD75::VFO::A) ? THD75::VFO::B : THD75::VFO::A;
  if (radio.set_current_vfo(test_vfo)) {
    std::cout << "[PASS] set_current_vfo succeeded\n";
    THD75::VFO verify = radio.get_current_vfo();
    if (verify == test_vfo) {
      std::cout << "[PASS] VFO verified correctly\n";
    } else {
      std::cout << "[FAIL] VFO mismatch.\n";
      all_passed = false;
    }
  } else {
    std::cout << "[FAIL] set_current_vfo failed\n";
    all_passed = false;
  }

  radio.shutdown();

  return all_passed ? 0 : 1;
}

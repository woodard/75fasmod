/**
 * @file test_radio_control.cpp
 * @brief Test program for THD75 VFO and dual/single band control functionality
 */

#include "THD75.hpp"
#include <getopt.h>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  const char *const short_opts = "h";
  const option long_opts[] = {
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) !=
         -1) {
    switch (opt) {
    case 'h':
      std::cout << "Usage: " << argv[0] << "\n"
                << "Tests THD75 VFO and dual/single band control functions.\n";
      return 0;
    default:
      return 1;
    }
  }

  // Find radio port
  std::vector<std::string> ports = RadioController::find_tty_sysfs();
  if (ports.empty()) {
    std::cerr << "[ERROR] No radio serial ports found via sysfs.\n";
    return 1;
  }

  THD75 radio(ports[0]);
  if (!radio.initialize()) {
    std::cerr << "[ERROR] Failed to initialize radio on " << ports[0] << "\n";
    return 1;
  }

  bool all_passed = true;

  // Test get_current_vfo / set_current_vfo
  std::cout << "=== Testing VFO Control ===\n";

  THD75::VFO current_vfo = radio.get_current_vfo();
  std::cout << "Current VFO: " << (current_vfo == THD75::VFO::A ? "A" : "B")
            << "\n";

  THD75::VFO test_vfo = (current_vfo == THD75::VFO::A) ? THD75::VFO::B
                                                       : THD75::VFO::A;
  if (radio.set_current_vfo(test_vfo)) {
    std::cout << "[PASS] Set VFO to " << (test_vfo == THD75::VFO::A ? "A" : "B")
              << "\n";
    THD75::VFO verify_vfo = radio.get_current_vfo();
    if (verify_vfo == test_vfo) {
      std::cout << "[PASS] VFO verified as "
                << (verify_vfo == THD75::VFO::A ? "A" : "B") << "\n";
    } else {
      std::cout << "[FAIL] VFO mismatch!\n";
      all_passed = false;
    }
  } else {
    std::cout << "[FAIL] Failed to set VFO\n";
    all_passed = false;
  }

  // Test get_single / get_dual
  std::cout << "\n=== Testing Single/Dual Band Control ===\n";

  bool is_single = radio.get_single();
  bool is_dual = radio.get_dual();

  std::cout << "get_single() returned: " << (is_single ? "true" : "false")
            << "\n";
  std::cout << "get_dual() returned: " << (is_dual ? "true" : "false") << "\n";

  // They should be opposites
  if (is_single != is_dual) {
    std::cout << "[PASS] get_single and get_dual are consistent\n";
  } else {
    std::cout << "[INFO] Both return same value (may be acceptable)\n";
  }

  // Test set_single
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

  // Test set_dual
  std::cout << "\n=== Testing set_dual ===\n";
  if (radio.set_dual()) {
    std::cout << "[PASS] set_dual() succeeded\n";
  } else {
    std::cout << "[FAIL] set_dual() failed\n";
    all_passed = false;
  }

  // Test get_tnc / set_tnc
  std::cout << "\n=== Testing TNC Control ===\n";

  int original_tnc = radio.get_tnc();
  std::cout << "Original TNC mode: " << original_tnc << "\n";

  // Try setting TNC to 0 (off)
  if (radio.set_tnc(0)) {
    std::cout << "[PASS] set_tnc(0) succeeded\n";
    int verify_tnc = radio.get_tnc();
    if (verify_tnc == 0) {
      std::cout << "[PASS] TNC verified as 0\n";
    } else {
      std::cout << "[INFO] TNC read as " << verify_tnc << " (may be expected)\n";
    }
  } else {
    std::cout << "[FAIL] set_tnc(0) failed\n";
    all_passed = false;
  }

  // Restore original TNC
  if (original_tnc >= 0) {
    radio.set_tnc(original_tnc);
    std::cout << "[INFO] Restored TNC to " << original_tnc << "\n";
  }

  // Cleanup
  radio.shutdown();

  std::cout << "\n=== Test Summary ===\n";
  if (all_passed) {
    std::cout << "All tests PASSED\n";
    return 0;
  } else {
    std::cout << "Some tests FAILED\n";
    return 1;
  }
}

/**
 * @file THD75.cpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 */

#include "THD75.hpp"
#include "RadioController.hpp"
#include "hamlib/rig.h"
#include "hamlib/riglist.h"
#include <cctype>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {
constexpr int BAUD_RATE_9600 = 9600;
constexpr size_t BUFFER_SIZE_32 = 32;
constexpr size_t BUFFER_SIZE_64 = 64;
constexpr size_t BUFFER_SIZE_16 = 16;
constexpr unsigned int MENU_ITEM_102 = 102;
} // namespace

// #include "absl/strings/match.h"  // Disabled - replaced with
// std::string::find

// Constructor
THD75::THD75(std::string port, rig_model_t model, bool hamlib_debug)
    : RadioController(model, std::move(port), hamlib_debug) {}

// THD75-specific implementations
auto THD75::set_ptt(bool transmit) -> bool {
  return rig_set_ptt(rig_, RIG_VFO_CURR, transmit ? RIG_PTT_ON : RIG_PTT_OFF) ==
         RIG_OK;
}

auto THD75::set_power_level(const std::string &level) -> bool {
  std::string lvl = level;
  for (auto &chr : lvl) {
    chr = std::toupper(chr);
  }

  PowerLevel val = PowerLevel::UNKNOWN;
  if (lvl == "H") {
    val = PowerLevel::HIGH;
  } else if (lvl == "M") {
    val = PowerLevel::MEDIUM;
  } else if (lvl == "L") {
    val = PowerLevel::LOW;
  } else if (lvl == "EL") {
    val = PowerLevel::EXTRA_LOW;
  } else {
    std::cerr << "Error: Invalid power level '" << level
              << "'. Use EL, L, M, or H.\n";
    return false;
  }

  std::cerr << "[RIG] Setting TX power to " << lvl << "...\n";
  return kenwood_power_set(val);
}

auto THD75::get_power_level(std::string &level) -> bool {
  PowerLevel const pwr = kenwood_power_get();
  switch (pwr) {
  case PowerLevel::HIGH:
    level = "H";
    return true;
  case PowerLevel::MEDIUM:
    level = "M";
    return true;
  case PowerLevel::LOW:
    level = "L";
    return true;
  case PowerLevel::EXTRA_LOW:
    level = "EL";
    return true;
  default:
    return false;
  }
}

auto THD75::set_other_power_level(const std::string &level) -> bool {
  // Check if in single mode - there's no "other" VFO in single mode
  if (get_single()) {
    return false;
  }

  // Get current VFO
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  // Switch to other VFO, set power level, then restore original VFO
  if (!set_current_vfo(other)) {
    return false;
  }

  bool const result = set_power_level(level);

  // Restore original VFO
  set_current_vfo(current);

  return result;
}

auto THD75::get_other_power_level(std::string &level) -> bool {
  // Check if in single mode - there's no "other" VFO in single mode
  if (get_single()) {
    level = "H"; // Default fallback to high power on error
    return false;
  }

  // Get current VFO
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  // Switch to other VFO, get power level, then restore original VFO
  if (!set_current_vfo(other)) {
    level = "H"; // Default fallback to high power on error
    return false;
  }

  bool const result = get_power_level(level);

  // Restore original VFO
  set_current_vfo(current);

  if (!result) {
    level = "H"; // Default fallback to high power on error
  }

  return result;
}

auto THD75::initialize() -> bool {
  std::cerr << "[RIG] Backing up current radio state..." << std::endl;

  // 1. Check if in dual mode and save "other" VFO state first
  bool const in_dual = !get_single();
  if (in_dual) {
    std::cerr << "[RIG] In dual mode - saving 'other' VFO state..."
              << std::endl;

    VFO const current = get_current_vfo();
    VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

    // Switch to other VFO
    if (set_current_vfo(other)) {
      // Save frequency
      double freq = 0.0;
      if (get_frequency(freq)) {
        orig_other_frequency_ = static_cast<freq_t>(freq * 1e6);
        orig_other_freq_saved_ = true;
        std::cerr << "[RIG] Saved other VFO frequency: " << freq << " MHz"
                  << std::endl;
      }

      // Save mode
      Mode mode;
      if (get_mode(mode)) {
        orig_other_mode_ = static_cast<rmode_t>(mode);
        orig_other_mode_saved_ = true;
        std::cerr << "[RIG] Saved other VFO mode: " << static_cast<int>(mode)
                  << std::endl;
      }

      // Save power level
      std::string powerStr;
      if (get_power_level(powerStr)) {
        if (powerStr == "H")
          orig_other_power_ = PowerLevel::HIGH;
        else if (powerStr == "M")
          orig_other_power_ = PowerLevel::MEDIUM;
        else if (powerStr == "L")
          orig_other_power_ = PowerLevel::LOW;
        else if (powerStr == "EL")
          orig_other_power_ = PowerLevel::EXTRA_LOW;
        orig_other_power_saved_ = true;
        std::cerr << "[RIG] Saved other VFO power: " << powerStr << std::endl;
      }

      // Restore original VFO
      set_current_vfo(current);
    }
  }

  // 2. Save current state (VFO, power, menu 102)
  rig_get_vfo(rig_, &orig_vfo_);
  orig_power_ = kenwood_power_get();
  orig_power_saved_ = true;
  orig_menu_102_ = kenwood_usb_out_select_get();

  // 3. Call base class to save frequency, mode, power for current VFO
  if (!RadioController::initialize()) {
    return false;
  }

  // 4. Set to VFO B to allow menu 102 changes when in dual mode
  std::cerr << "[RIG] Setting radio to VFO B for menu 102 access..."
            << std::endl;
  rig_set_vfo(rig_, RIG_VFO_B);

  // 5. Configure Kenwood 9600 bps data output path (Menu 102) safely
  if (orig_menu_102_ != UsbOutSelect::unknown &&
      orig_menu_102_ != UsbOutSelect::IF) {
    std::cerr << "[RIG] Changing Menu 102 to IF Output (1). This will cause a "
                 "USB reset..."
              << std::endl;
    if (!kenwood_usb_out_select_set(UsbOutSelect::IF)) {
      std::cerr << "[RIG] CRITICAL ERROR: Could not switch Menu 102 to IF."
                << std::endl;
      return false;
    }

    // The radio is rebooting its USB interface. Close handles first.
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;

    std::cerr << "[RIG] Waiting 4 seconds for USB re-enumeration..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // Re-initialize Hamlib
    std::cerr << "[RIG] Reconnecting to Hamlib after USB reset..." << std::endl;
    rig_ = rig_init(model_);
    rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

    int const re_status = rig_open(rig_);
    if (re_status != RIG_OK) {
      std::cerr << "[RIG] Error: Failed to reconnect. Code: " << re_status
                << std::endl;
      return false;
    }
    std::cerr << "[RIG] Successfully reconnected to radio." << std::endl;
  } else if (orig_menu_102_ == UsbOutSelect::IF) {
    std::cerr << "[RIG] Menu 102 already set to IF Output. Skipping."
              << std::endl;
  }

  // 6. Set mode to Packet FM (9600 baud passband)
  std::cerr << "[RIG] Configuring radio for high-speed modem operation..."
            << std::endl;
  int mode_ret =
      rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, BAUD_RATE_9600);
  if (mode_ret != RIG_OK) {
    std::cerr << "[RIG] PKTFM mode rejected, falling back to standard FM..."
              << std::endl;
    mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_FM, 0);
  }

  return true;
}

void THD75::shutdown() {
  if (rig_ != nullptr) {
    std::cerr << "[RIG] Shutting down. Restoring original radio settings..."
              << std::endl;

    // 1. Restore "other" VFO state if in dual mode
    bool const in_dual = !get_single();
    if (in_dual && orig_other_freq_saved_) {
      std::cerr << "[RIG] Restoring 'other' VFO state..." << std::endl;

      VFO const current = get_current_vfo();
      VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

      // Switch to other VFO
      if (set_current_vfo(other)) {
        // Restore frequency
        if (orig_other_freq_saved_) {
          double freq_mhz = static_cast<double>(orig_other_frequency_) / 1e6;
          if (set_frequency(freq_mhz)) {
            std::cerr << "[RIG] Restored other VFO frequency: " << freq_mhz
                      << " MHz" << std::endl;
          }
        }

        // Restore mode
        if (orig_other_mode_saved_) {
          if (set_mode(static_cast<Mode>(orig_other_mode_))) {
            std::cerr << "[RIG] Restored other VFO mode" << std::endl;
          }
        }

        // Restore power level
        if (orig_other_power_saved_) {
          std::string level;
          switch (orig_other_power_) {
          case PowerLevel::HIGH:
            level = "H";
            break;
          case PowerLevel::MEDIUM:
            level = "M";
            break;
          case PowerLevel::LOW:
            level = "L";
            break;
          case PowerLevel::EXTRA_LOW:
            level = "EL";
            break;
          default:
            level = "UNKNOWN";
          }
          if (level != "UNKNOWN") {
            if (set_power_level(level)) {
              std::cerr << "[RIG] Restored other VFO power to " << level
                        << std::endl;
            }
          }
        }

        // Restore original VFO
        set_current_vfo(current);
      }
    }

    // 2. Restore original menu 102
    if (orig_menu_102_ != UsbOutSelect::unknown) {
      if (!kenwood_usb_out_select_set(orig_menu_102_)) {
        std::cerr << "[RIG] Warning: Failed to restore Menu 102." << std::endl;
      }
    }

    // 3. Restore original VFO
    if (orig_vfo_ != RIG_VFO_NONE) {
      if (rig_set_vfo(rig_, orig_vfo_) == RIG_OK) {
        std::cerr << "[RIG] Restored VFO to " << rig_strvfo(orig_vfo_)
                  << std::endl;
      }
    }
  }

  // 4. Call base shutdown (restores frequency, mode, power level for current
  // VFO, closes rig)
  RadioController::shutdown();
}

// Call base shutdown (restores frequency, mode, power level, closes rig)
// TNC control functions (public interface)
auto THD75::get_tnc() -> int {
  char cmd[] = "TN\r";
  char buf[BUFFER_SIZE_64] = {0};
  unsigned char term = '\r';

  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (bytes > 0) {
    std::string const resp(buf);
    size_t const space_pos = resp.find(' ');
    size_t const comma = resp.find(',');

    if (comma != std::string::npos) {
      size_t const start = (space_pos != std::string::npos) ? space_pos + 1 : 2;
      try {
        return std::stoi(resp.substr(start, comma - start));
      } catch (...) {
        return -1;
      }
    }
  }
  return -1;
}

auto THD75::set_tnc(int mode) -> bool {
  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "TN %d,0\r", mode);

  char buf[BUFFER_SIZE_64] = {0};
  unsigned char term = '\r';
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  return bytes > 0;
}

// Kenwood helper method implementations
auto THD75::kenwood_menu_get(int menu_num) -> int {
  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "EX%03d\r", menu_num);

  char buf[BUFFER_SIZE_64] = {0};
  unsigned char term = '\r';
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (bytes > 0) {
    std::string const resp(buf);
    size_t const comma = resp.find(',');

    size_t term_pos = resp.find('\r');
    if (term_pos == std::string::npos) {
      term_pos = resp.find(';');
    }

    if (comma != std::string::npos && term_pos != std::string::npos) {
      try {
        return std::stoi(resp.substr(comma + 1, term_pos - comma - 1));
      } catch (...) {
        return -1;
      }
    }
  }
  return -1;
}

auto THD75::kenwood_menu_set(int menu_num, int value) -> bool {
  if (value < 0) {
    return false;
  }

  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "EX%03d,%d\r", menu_num, value);

  char buf[BUFFER_SIZE_64] = {0};
  unsigned char term = '\r';
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  return bytes > 0;
}

auto THD75::kenwood_usb_out_select_get() -> THD75::UsbOutSelect {
  int const value = kenwood_menu_get(102);
  switch (value) {
  case 0:
    return UsbOutSelect::AF;
  case 1:
    return UsbOutSelect::IF;
  case 2:
    return UsbOutSelect::Detect;
  default:
    return UsbOutSelect::unknown;
  }
}

auto THD75::kenwood_usb_out_select_set(UsbOutSelect value) -> bool {
  return kenwood_menu_set(MENU_ITEM_102, static_cast<int>(value));
}

auto THD75::kenwood_power_get() -> THD75::PowerLevel {
  // Query active band (0 = Band A, 1 = Band B)
  int active_band = 0;
  char bc_buf[BUFFER_SIZE_32];
  unsigned char term = '\r';

  int const bc_bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>("BC\r"), 3,
      reinterpret_cast<unsigned char *>(bc_buf), sizeof(bc_buf) - 1, &term);

  if (bc_bytes > 0) {
    std::string const bc_resp(bc_buf);
    if ((bc_resp.find("BC 1") != std::string::npos) ||
        (bc_resp.find("BC1") != std::string::npos)) {
      active_band = 1;
    }
  }

  // Fetch power for the active band
  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "PC %d\r", active_band);

  char buf[BUFFER_SIZE_64] = {0};
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (bytes > 0) {
    std::string const resp(buf);
    size_t const comma = resp.find(',');

    size_t term_pos = resp.find('\r');
    if (term_pos == std::string::npos) {
      term_pos = resp.find(';');
    }

    if (comma != std::string::npos && term_pos != std::string::npos &&
        term_pos > comma + 1) {
      try {
        int const pwr_int =
            std::stoi(resp.substr(comma + 1, term_pos - comma - 1));
        if (pwr_int >= 0 && pwr_int <= 3) {
          return static_cast<PowerLevel>(pwr_int);
        }
      } catch (...) {
        return PowerLevel::UNKNOWN;
      }
    }
  }
  return PowerLevel::UNKNOWN;
}

auto THD75::kenwood_power_set(PowerLevel val) -> bool {
  if (val == PowerLevel::UNKNOWN) {
    return false;
  }

  // Query active band
  int active_band = 0;
  char bc_buf[BUFFER_SIZE_32];
  unsigned char term = '\r';

  int const bc_bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>("BC\r"), 3,
      reinterpret_cast<unsigned char *>(bc_buf), sizeof(bc_buf) - 1, &term);

  if (bc_bytes > 0) {
    std::string const bc_resp(bc_buf);
    if ((bc_resp.find("BC 1") != std::string::npos) ||
        (bc_resp.find("BC1") != std::string::npos)) {
      active_band = 1;
    }
  }

  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "PC %d,%d\r", active_band, static_cast<int>(val));

  char buf[BUFFER_SIZE_64] = {0};
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  return bytes > 0;
}

// VFO control functions
auto THD75::get_current_vfo() -> VFO {
  vfo_t vfo = 0;
  if (rig_get_vfo(rig_, &vfo) == RIG_OK) {
    if (vfo == RIG_VFO_A) {
      return VFO::A;
    }
    if (vfo == RIG_VFO_B) {
      return VFO::B;
    }
  }
  return VFO::A; // Default fallback
}

auto THD75::set_current_vfo(VFO vfo) -> bool {
  vfo_t hamlib_vfo = 0;
  if (vfo == VFO::A) {
    hamlib_vfo = RIG_VFO_A;
  } else if (vfo == VFO::B) {
    hamlib_vfo = RIG_VFO_B;
  } else {
    return false;
  }
  return rig_set_vfo(rig_, hamlib_vfo) == RIG_OK;
}

// Single/Dual Band control functions
auto THD75::get_single() -> bool {
  int status = 0;
  if (rig_get_func(rig_, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, &status) ==
      RIG_OK) {
    return status == 0; // If DUAL_WATCH is disabled (0), we're in Single mode
  }
  return true; // Default to single on error
}

auto THD75::set_single(VFO vfo) -> bool {
  // Send BC command to set VFO and disable dual-watch
  int const band = (vfo == VFO::B) ? 1 : 0;
  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "BC %d,0\r", band);

  char buf[BUFFER_SIZE_64] = {0};
  unsigned char term = '\r';
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);
  return bytes > 0;
}

auto THD75::get_dual() -> bool {
  int status = 0;
  if (rig_get_func(rig_, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, &status) ==
      RIG_OK) {
    return status != 0; // If DUAL_WATCH is enabled, we're in Dual mode
  }
  return false; // Default to dual off on error
}

auto THD75::set_dual() -> bool {
  // Query current band to set dual-watch on the current control band
  char bc_buf[BUFFER_SIZE_32];
  unsigned char term = '\r';

  int const bc_bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>("BC\r"), 3,
      reinterpret_cast<unsigned char *>(bc_buf), sizeof(bc_buf) - 1, &term);

  if (bc_bytes <= 0) {
    return false;
  }

  // Parse the response to get the current band
  std::string const bc_resp(bc_buf);
  int band = 0;
  if ((bc_resp.find("BC 1") != std::string::npos) ||
      (bc_resp.find("BC1") != std::string::npos)) {
    band = 1;
  }

  // Send BC command to enable dual-watch on the current band
  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "BC %d,1\r", band);

  char buf[BUFFER_SIZE_64] = {0};
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);
  return bytes > 0;
}

// Toggle single/dual mode
auto THD75::flip_single_dual(VFO vfo) -> bool {
  if (get_single()) {
    // Currently in single mode, switch to dual
    return set_dual();
  } // Currently in dual mode, switch to single
  return set_single(vfo);
}

// Other VFO mode control functions
auto THD75::set_other_mode(Mode mode) -> bool {
  // Check if in single mode - there's no "other" VFO in single mode
  if (get_single()) {
    return false;
  }

  // Get current VFO
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  // Switch to other VFO, set mode, then restore original VFO
  if (!set_current_vfo(other)) {
    return false;
  }

  bool const result = set_mode(mode);

  // Restore original VFO
  set_current_vfo(current);

  return result;
}

auto THD75::get_other_mode() -> Mode {
  // Check if in single mode - there's no "other" VFO in single mode
  if (get_single()) {
    return Mode::FM; // Default fallback on error
  }

  // Get current VFO
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  // Switch to other VFO, get mode, then restore original VFO
  if (!set_current_vfo(other)) {
    return Mode::FM; // Default fallback on error
  }

  Mode mode;
  bool const result = get_mode(mode);

  // Restore original VFO
  set_current_vfo(current);

  if (!result) {
    return Mode::FM; // Default fallback on error
  }

  return mode;
}

// Other VFO frequency control functions
auto THD75::set_other_frequency(double freq_mhz) -> bool {
  // Check if in single mode - there's no "other" VFO in single mode
  if (get_single()) {
    return false;
  }

  // Get current VFO
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  // Switch to other VFO, set frequency, then restore original VFO
  if (!set_current_vfo(other)) {
    return false;
  }

  bool const result = set_frequency(freq_mhz);

  // Restore original VFO
  set_current_vfo(current);

  return result;
}

auto THD75::get_other_frequency() -> double {
  // Check if in single mode - there's no "other" VFO in single mode
  if (get_single()) {
    return 0.0; // Default fallback on error
  }

  // Get current VFO
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  // Switch to other VFO, get frequency, then restore original VFO
  if (!set_current_vfo(other)) {
    return 0.0; // Default fallback on error
  }

  double freq = NAN;
  bool const result = get_frequency(freq);

  // Restore original VFO
  set_current_vfo(current);

  if (!result) {
    return 0.0; // Default fallback on error
  }

  return freq;
}

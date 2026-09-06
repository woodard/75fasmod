/**
 * @file THD75.cpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 */

#include "THD75.hpp"
#include <chrono>
#include <iostream>
#include <thread>

// Constructor
THD75::THD75(const std::string &port, rig_model_t model, bool hamlib_debug)
    : RadioController(model, port, hamlib_debug),
      orig_menu_102_(UsbOutSelect::unknown), orig_vfo_(RIG_VFO_NONE) {}

// THD75-specific implementations
auto THD75::set_ptt(bool transmit) -> bool {
  return rig_set_ptt(rig_, RIG_VFO_CURR, transmit ? RIG_PTT_ON : RIG_PTT_OFF) ==
         RIG_OK;
}

auto THD75::set_power_level(const std::string &level) -> bool {
  std::string lvl = level;
  for (auto &chr : lvl)
    chr = std::toupper(chr);

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
  PowerLevel pwr = kenwood_power_get();
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

bool THD75::initialize() {
  std::cerr << "[RIG] Backing up current radio state...\n";

  // 1. Set to VFO B to allow menu 102 changes when in dual mode
  std::cerr << "[RIG] Setting radio to VFO B for menu 102 access...\n";
  rig_set_vfo(rig_, RIG_VFO_B);

  // 2. Query power and Menu 102 state
  orig_power_ = kenwood_power_get();
  orig_menu_102_ = kenwood_usb_out_select_get();

  if (orig_menu_102_ == UsbOutSelect::unknown) {
    std::cerr
        << "\n[RIG] WARNING: Could not query Menu 102 via CAT.\n"
        << "      Kenwood locks this menu over USB. Please manually "
        << "ensure Menu 102 (USB Out Select) is set to IF Output (1).\n\n";
  }

  std::cerr << "[RIG] Configuring radio for high-speed modem operation...\n";

  // 3. Set mode to Packet FM (9600 baud passband)
  int mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, 9600);
  if (mode_ret != RIG_OK) {
    std::cerr << "[RIG] PKTFM mode rejected, falling back to standard FM...\n";
    mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_FM, 0);
  }

  // 4. Configure Kenwood 9600 bps data output path (Menu 102) safely
  if (orig_menu_102_ != UsbOutSelect::unknown &&
      orig_menu_102_ != UsbOutSelect::IF) {
    std::cerr << "[RIG] Changing Menu 102 to IF Output (1). This will cause a "
                 "USB reset...\n";
    if (!kenwood_usb_out_select_set(UsbOutSelect::IF)) {
      std::cerr << "[RIG] CRITICAL ERROR: Could not switch Menu 102 to IF.\n";
      return false;
    }

    // The radio is rebooting its USB interface. Close handles first.
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;

    std::cerr << "[RIG] Waiting 4 seconds for USB re-enumeration...\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // Re-initialize Hamlib
    std::cerr << "[RIG] Reconnecting to Hamlib after USB reset...\n";
    rig_ = rig_init(model_);
    rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

    int re_status = rig_open(rig_);
    if (re_status != RIG_OK) {
      std::cerr << "[RIG] Error: Failed to reconnect. Code: " << re_status
                << "\n";
      return false;
    }
    std::cerr << "[RIG] Successfully reconnected to radio.\n";
  } else if (orig_menu_102_ == UsbOutSelect::IF) {
    std::cerr << "[RIG] Menu 102 already set to IF Output. Skipping.\n";
  }

  return true;
}

void THD75::shutdown() {
  if (rig_) {
    std::cerr << "[RIG] Shutting down. Restoring original radio settings...\n";

    // Restore original menu 102
    if (orig_menu_102_ != UsbOutSelect::unknown) {
      if (!kenwood_usb_out_select_set(orig_menu_102_)) {
        std::cerr << "[RIG] Warning: Failed to restore Menu 102.\n";
      }
    }

    // Restore original VFO
    if (orig_vfo_ != RIG_VFO_NONE) {
      if (rig_set_vfo(rig_, orig_vfo_) == RIG_OK) {
        std::cerr << "[RIG] Restored VFO to " << rig_strvfo(orig_vfo_) << "\n";
      }
    }

    // Restore original operating mode and bandwidth
    if (orig_mode_ != RIG_MODE_NONE) {
      std::cerr << "[RIG] Restoring original mode...\n";
      rig_set_mode(rig_, RIG_VFO_CURR, orig_mode_, orig_width_);
    }
  }

  // Call base shutdown (closes rig)
  RadioController::shutdown();
}

// TNC control functions (public interface)
auto THD75::get_tnc() -> int {
  char cmd[] = "TN\r";
  char buf[64] = {0};
  unsigned char term = '\r';

  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    size_t space_pos = resp.find(' ');
    size_t comma = resp.find(',');

    if (comma != std::string::npos) {
      size_t start = (space_pos != std::string::npos) ? space_pos + 1 : 2;
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
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "TN %d,0\r", mode);

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  return bytes > 0;
}

// Kenwood helper method implementations
int THD75::kenwood_menu_get(int menu_num) {
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "EX%03d\r", menu_num);

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    size_t comma = resp.find(',');

    size_t term_pos = resp.find('\r');
    if (term_pos == std::string::npos)
      term_pos = resp.find(';');

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

bool THD75::kenwood_menu_set(int menu_num, int value) {
  if (value < 0)
    return false;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "EX%03d,%d\r", menu_num, value);

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  return bytes > 0;
}

THD75::UsbOutSelect THD75::kenwood_usb_out_select_get() {
  int value = kenwood_menu_get(102);
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

bool THD75::kenwood_usb_out_select_set(UsbOutSelect value) {
  return kenwood_menu_set(102, static_cast<int>(value));
}

THD75::PowerLevel THD75::kenwood_power_get() {
  // Query active band (0 = Band A, 1 = Band B)
  int active_band = 0;
  char bc_buf[32] = {0};
  unsigned char term = '\r';

  int bc_bytes =
      rig_send_raw(rig_, (const unsigned char *)"BC\r", 3,
                   (unsigned char *)bc_buf, sizeof(bc_buf) - 1, &term);

  if (bc_bytes > 0) {
    std::string bc_resp(bc_buf);
    if (bc_resp.find("BC 1") != std::string::npos ||
        bc_resp.find("BC1") != std::string::npos) {
      active_band = 1;
    }
  }

  // Fetch power for the active band
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "PC %d\r", active_band);

  char buf[64] = {0};
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    size_t comma = resp.find(',');

    size_t term_pos = resp.find('\r');
    if (term_pos == std::string::npos)
      term_pos = resp.find(';');

    if (comma != std::string::npos && term_pos != std::string::npos &&
        term_pos > comma + 1) {
      try {
        int pwr_int = std::stoi(resp.substr(comma + 1, term_pos - comma - 1));
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

bool THD75::kenwood_power_set(PowerLevel val) {
  if (val == PowerLevel::UNKNOWN)
    return false;

  // Query active band
  int active_band = 0;
  char bc_buf[32] = {0};
  unsigned char term = '\r';

  int bc_bytes =
      rig_send_raw(rig_, (const unsigned char *)"BC\r", 3,
                   (unsigned char *)bc_buf, sizeof(bc_buf) - 1, &term);

  if (bc_bytes > 0) {
    std::string bc_resp(bc_buf);
    if (bc_resp.find("BC 1") != std::string::npos ||
        bc_resp.find("BC1") != std::string::npos) {
      active_band = 1;
    }
  }

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "PC %d,%d\r", active_band, static_cast<int>(val));

  char buf[64] = {0};
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  return bytes > 0;
}

// VFO control functions
auto THD75::get_current_vfo() -> VFO {
  vfo_t vfo;
  if (rig_get_vfo(rig_, &vfo) == RIG_OK) {
    if (vfo == RIG_VFO_A) {
      return VFO::A;
    } else if (vfo == RIG_VFO_B) {
      return VFO::B;
    }
  }
  return VFO::A; // Default fallback
}

auto THD75::set_current_vfo(VFO vfo) -> bool {
  vfo_t hamlib_vfo;
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
  int band = (vfo == VFO::B) ? 1 : 0;
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "BC %d,0\r", band);

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);
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
  char bc_buf[32] = {0};
  unsigned char term = '\r';

  int bc_bytes = rig_send_raw(rig_, (const unsigned char *)"BC\r", 3,
                              (unsigned char *)bc_buf, sizeof(bc_buf) - 1, &term);

  if (bc_bytes <= 0) {
    return false;
  }

  // Parse the response to get the current band
  std::string bc_resp(bc_buf);
  int band = 0;
  if (bc_resp.find("BC 1") != std::string::npos ||
      bc_resp.find("BC1") != std::string::npos) {
    band = 1;
  }

  // Send BC command to enable dual-watch on the current band
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "BC %d,1\r", band);

  char buf[64] = {0};
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);
  return bytes > 0;
}

// Toggle single/dual mode
auto THD75::flip_single_dual(VFO vfo) -> bool {
  if (get_single()) {
    // Currently in single mode, switch to dual
    return set_dual();
  } else {
    // Currently in dual mode, switch to single
    return set_single(vfo);
  }
}

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
constexpr size_t BUFFER_SIZE_16 = 16;
constexpr size_t BUFFER_SIZE_32 = 32;
constexpr size_t response_buffer_size = 256;
constexpr unsigned int MENU_ITEM_102 = 102;

/**
 * @brief Validates that the radio response was received and not rejected by CAT
 *
 * Checks that response length is valid and does not start with '?' (Syntax error)
 * or 'N' (NACK/Refused).
 */
bool is_valid_cat_response(int bytes, const char *buf) {
  return bytes > 0 && buf[0] != '?' && buf[0] != 'N';
}
} // namespace

// Constructor
THD75::THD75(std::string port, rig_model_t model, bool hamlib_debug)
    : RadioController(model, std::move(port), hamlib_debug) {}

/**
 * @brief Set the power level on the radio
 *
 * @param level Power level string ("EL", "L", "M", or "H")
 * @return true if successful, false otherwise
 */
auto THD75::set_power_level(const std::string &level) -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  std::string lvl = level;
  for (auto &chr : lvl) {
    chr = static_cast<char>(std::toupper(static_cast<unsigned char>(chr)));
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

/**
 * @brief Get the current power level from the radio
 *
 * @param level Reference to store the power level string ("EL", "L", "M", "H")
 * @return true if successful, false otherwise
 */
auto THD75::get_power_level(std::string &level) -> bool {
  if (rig_ == nullptr) {
    return false;
  }

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

/**
 * @brief Set the power level on the other VFO (not currently transmitting)
 *
 * @param level Power level string ("EL", "L", "M", or "H")
 * @return true if successful, false otherwise
 */
auto THD75::set_other_power_level(const std::string &level) -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  if (get_single()) {
    return false;
  }

  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  if (!set_current_vfo(other)) {
    return false;
  }

  bool const result = set_power_level(level);
  set_current_vfo(current);

  return result;
}

/**
 * @brief Get the power level from the other VFO (not currently transmitting)
 *
 * @param level Reference to store the power level string ("EL", "L", "M", "H")
 * @return true if successful, false otherwise
 */
auto THD75::get_other_power_level(std::string &level) -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  if (get_single()) {
    level = "H";
    return false;
  }

  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  if (!set_current_vfo(other)) {
    level = "H";
    return false;
  }

  bool const result = get_power_level(level);
  set_current_vfo(current);

  if (!result) {
    level = "H";
  }

  return result;
}

/**
 * @brief Initialize the THD75 radio controller
 *
 * Saves current radio settings and configures the radio for
 * high-speed modem operation by setting USB Out Select to IF.
 *
 * @return true if initialization successful, false otherwise
 */
auto THD75::initialize() -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  std::cerr << "[RIG] Backing up current radio state..." << std::endl;

  bool const in_dual = get_dual();
  if (in_dual) {
    std::cerr << "[RIG] In dual mode - saving 'other' VFO state..."
              << std::endl;

    VFO const current = get_current_vfo();
    VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

    if (set_current_vfo(other)) {
      double freq = 0.0;
      if (get_frequency(freq)) {
        orig_other_frequency_ = static_cast<freq_t>(freq * 1e6);
        orig_other_freq_saved_ = true;
        std::cerr << "[RIG] Saved other VFO frequency: " << freq << " MHz"
                  << std::endl;
      }

      Mode mode;
      if (get_mode(mode)) {
        orig_other_mode_ = static_cast<rmode_t>(mode);
        orig_other_mode_saved_ = true;
        std::cerr << "[RIG] Saved other VFO mode: " << static_cast<int>(mode)
                  << std::endl;
      }

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

      set_current_vfo(current);
    }
  }

  orig_vfo_ = (get_current_vfo() == VFO::A) ? RIG_VFO_A : RIG_VFO_B;
  orig_power_ = kenwood_power_get();
  orig_power_saved_ = true;
  orig_menu_102_ = kenwood_usb_out_select_get();

  double cur_freq = 0.0;
  if (get_frequency(cur_freq)) {
    orig_frequency_ = static_cast<freq_t>(cur_freq * 1e6);
    orig_frequency_saved_ = true;
  }
  Mode cur_mode;
  if (get_mode(cur_mode)) {
    orig_mode_ = static_cast<rmode_t>(cur_mode);
    orig_mode_saved_ = true;
  }

  std::cerr << "[RIG] Setting radio to VFO B for menu 102 access..."
            << std::endl;
  set_current_vfo(VFO::B);

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

    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;

    std::cerr << "[RIG] Waiting 4 seconds for USB re-enumeration..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::cerr << "[RIG] Reconnecting to Hamlib after USB reset..." << std::endl;
    rig_ = rig_init(model_);
    rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

    int const re_status = rig_open(rig_);
    if (re_status != RIG_OK) {
      std::cerr << "[RIG] Error: Failed to reconnect. Code: " << re_status
                << std::endl;
      return false;
    }
    flush_serial();
    std::cerr << "[RIG] Successfully reconnected to radio." << std::endl;
  } else if (orig_menu_102_ == UsbOutSelect::IF) {
    std::cerr << "[RIG] Menu 102 already set to IF Output. Skipping."
              << std::endl;
  }

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

/**
 * @brief Shutdown the THD75 radio controller
 *
 * Restores the radio to its original state after modifications.
 */
void THD75::shutdown() {
  if (rig_ != nullptr) {
    std::cerr << "[RIG] Shutting down. Restoring original radio settings..."
              << std::endl;

    bool const in_dual = get_dual();
    if (in_dual && orig_other_freq_saved_) {
      std::cerr << "[RIG] Restoring 'other' VFO state..." << std::endl;

      VFO const current = get_current_vfo();
      VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

      if (set_current_vfo(other)) {
        if (orig_other_freq_saved_) {
          double freq_mhz = static_cast<double>(orig_other_frequency_) / 1e6;
          if (set_frequency(freq_mhz)) {
            std::cerr << "[RIG] Restored other VFO frequency: " << freq_mhz
                      << " MHz" << std::endl;
          }
        }

        if (orig_other_mode_saved_) {
          if (set_mode(static_cast<Mode>(orig_other_mode_))) {
            std::cerr << "[RIG] Restored other VFO mode" << std::endl;
          }
        }

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

        set_current_vfo(current);
      }
    }

    if (orig_menu_102_ != UsbOutSelect::unknown) {
      if (!kenwood_usb_out_select_set(orig_menu_102_)) {
        std::cerr << "[RIG] Warning: Failed to restore Menu 102." << std::endl;
      }
    }

    if (orig_vfo_ != RIG_VFO_NONE) {
      set_current_vfo((orig_vfo_ == RIG_VFO_A) ? VFO::A : VFO::B);
    }
  }

  RadioController::shutdown();
}

/**
 * @brief Get the Kenwood TNC mode
 *
 * @return TNC mode status
 */
auto THD75::get_tnc() -> int {
  if (rig_ == nullptr) {
    return -1;
  }

  char cmd[] = "TN;\r";
  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';

  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (is_valid_cat_response(bytes, buf)) {
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

/**
 * @brief Set the Kenwood TNC mode
 *
 * @param mode The TNC mode to set
 * @return true if successful, false otherwise
 */
auto THD75::set_tnc(int mode) -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "TN %d,0;\r", mode);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  return is_valid_cat_response(bytes, buf);
}

// Kenwood helper method implementations
auto THD75::kenwood_menu_get(int menu_num) -> int {
  if (rig_ == nullptr) {
    return -1;
  }

  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "EX%03d;\r", menu_num);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (is_valid_cat_response(bytes, buf)) {
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
  if (rig_ == nullptr) {
    return false;
  }

  if (value < 0) {
    return false;
  }

  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "EX%03d,%d;\r", menu_num, value);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  return is_valid_cat_response(bytes, buf);
}

auto THD75::kenwood_usb_out_select_get() -> THD75::UsbOutSelect {
  if (rig_ == nullptr) {
    return UsbOutSelect::unknown;
  }

  int const value = kenwood_menu_get(MENU_ITEM_102);
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
  if (rig_ == nullptr) {
    return false;
  }

  return kenwood_menu_set(MENU_ITEM_102, static_cast<int>(value));
}

auto THD75::kenwood_power_get() -> THD75::PowerLevel {
  if (rig_ == nullptr) {
    return PowerLevel::UNKNOWN;
  }

  int active_band = (get_current_vfo() == VFO::B) ? 1 : 0;

  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "PC %d;\r", active_band);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (is_valid_cat_response(bytes, buf)) {
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
  if (rig_ == nullptr || val == PowerLevel::UNKNOWN) {
    return false;
  }

  int active_band = (get_current_vfo() == VFO::B) ? 1 : 0;

  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "PC %d,%d;\r", active_band, static_cast<int>(val));

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  return is_valid_cat_response(bytes, buf);
}

/**
 * @brief Get the current VFO that will transmit when PTT is set
 *
 * Query the radio's band control (BC) status to determine if Band A or B is selected.
 *
 * @return VFO::A if VFO A is active, VFO::B if VFO B is active
 */
auto THD75::get_current_vfo() -> VFO {
  if (rig_ == nullptr) {
    return VFO::A;
  }
  char bc_buf[BUFFER_SIZE_32] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bc_bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>("BC;\r"), 4,
      reinterpret_cast<unsigned char *>(bc_buf), sizeof(bc_buf) - 1, &term);

  if (is_valid_cat_response(bc_bytes, bc_buf)) {
    std::string const bc_resp(bc_buf);
    if (bc_resp.find("BC 1") != std::string::npos ||
        bc_resp.find("BC1") != std::string::npos) {
      return VFO::B;
    }
  }
  return VFO::A;
}

/**
 * @brief Set the VFO that will transmit when PTT is set
 *
 * Uses native Kenwood BC command to select the control band.
 *
 * @param vfo The VFO to set (VFO::A or VFO::B)
 * @return true if successful, false otherwise
 */
auto THD75::set_current_vfo(VFO vfo) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  int const band = (vfo == VFO::B) ? 1 : 0;
  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "BC %d;\r", band);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  bool const success = is_valid_cat_response(bytes, buf);
  flush_serial();
  return success;
}

/**
 * @brief Check if the radio is in Single Band mode
 *
 * @return true if in Single Band mode, false if in Dual Band mode
 */
auto THD75::get_single() -> bool {
  return !get_dual();
}

/**
 * @brief Set the radio to Single Band mode
 *
 * Uses native Kenwood BC command ("BC <band>,0;") to disable Dual Watch.
 *
 * @param vfo The VFO to use (VFO::A or VFO::B)
 * @return true if successful, false otherwise
 */
auto THD75::set_single(VFO vfo) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  int const band = (vfo == VFO::B) ? 1 : 0;
  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "BC %d,0;\r", band);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  bool const success = is_valid_cat_response(bytes, buf);
  flush_serial();
  return success;
}

/**
 * @brief Check if the radio is in Dual Band mode
 *
 * Queries BC command and checks the second parameter value.
 *
 * @return true if in Dual Band mode, false if in Single Band mode
 */
auto THD75::get_dual() -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  char bc_buf[BUFFER_SIZE_32] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bc_bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>("BC;\r"), 4,
      reinterpret_cast<unsigned char *>(bc_buf), sizeof(bc_buf) - 1, &term);

  if (is_valid_cat_response(bc_bytes, bc_buf)) {
    std::string const bc_resp(bc_buf);
    size_t comma = bc_resp.find(',');
    if (comma != std::string::npos && comma + 1 < bc_resp.length()) {
      return bc_resp[comma + 1] == '1';
    }
  }
  return false;
}

/**
 * @brief Set the radio to Dual Band mode
 *
 * Uses native Kenwood BC command ("BC <band>,1;") to enable Dual Watch.
 *
 * @return true if successful, false otherwise
 */
auto THD75::set_dual() -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  VFO const current = get_current_vfo();
  int const band = (current == VFO::B) ? 1 : 0;

  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "BC %d,1;\r", band);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  bool const success = is_valid_cat_response(bytes, buf);
  flush_serial();
  return success;
}

/**
 * @brief Set single or dual mode based on current state
 *
 * @param vfo The VFO to set if switching to single mode
 * @return true if successful, false otherwise
 */
auto THD75::flip_single_dual(VFO vfo) -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  if (get_single()) {
    return set_dual();
  }
  return set_single(vfo);
}

/**
 * @brief Set the mode on the other VFO (not currently transmitting)
 *
 * @param mode The mode to set on the other VFO
 * @return true if successful, false otherwise
 */
auto THD75::set_other_mode(Mode mode) -> bool {
  if (rig_ == nullptr || get_single()) {
    return false;
  }

  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  if (!set_current_vfo(other)) {
    return false;
  }

  bool const result = set_mode(mode);
  set_current_vfo(current);

  return result;
}

/**
 * @brief Get the mode from the other VFO (not currently transmitting)
 *
 * @return Mode from the other VFO
 */
auto THD75::get_other_mode() -> Mode {
  if (rig_ == nullptr || get_single()) {
    return Mode::FM;
  }

  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  if (!set_current_vfo(other)) {
    return Mode::FM;
  }

  Mode mode;
  bool const result = get_mode(mode);
  set_current_vfo(current);

  if (!result) {
    return Mode::FM;
  }

  return mode;
}

/**
 * @brief Set the frequency on the other VFO (not currently transmitting)
 *
 * @param freq_mhz The frequency in megahertz to set on the other VFO
 * @return true if successful, false otherwise
 */
auto THD75::set_other_frequency(double freq_mhz) -> bool {
  if (rig_ == nullptr || get_single()) {
    return false;
  }

  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  if (!set_current_vfo(other)) {
    return false;
  }

  bool const result = set_frequency(freq_mhz);
  set_current_vfo(current);

  return result;
}

/**
 * @brief Get the frequency from the other VFO (not currently transmitting)
 *
 * @return Frequency in megahertz from the other VFO
 */
auto THD75::get_other_frequency() -> double {
  if (rig_ == nullptr || get_single()) {
    return 0.0;
  }

  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;

  if (!set_current_vfo(other)) {
    return 0.0;
  }

  double freq = NAN;
  bool const result = get_frequency(freq);
  set_current_vfo(current);

  if (!result) {
    return 0.0;
  }

  return freq;
}

/**
 * @brief Get the current VFO frequency with TH-D75 CAT fallback
 *
 * Issues direct Kenwood FQ CAT query ("FQ <band>;\r") to avoid Hamlib FO parsing bugs.
 */
auto THD75::get_frequency(double &freq_mhz) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  int active_band = (get_current_vfo() == VFO::B) ? 1 : 0;

  char cmd[BUFFER_SIZE_16];
  snprintf(cmd, sizeof(cmd), "FQ %d;\r", active_band);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  if (is_valid_cat_response(bytes, buf)) {
    std::string const resp(buf);
    size_t const comma = resp.find(',');
    if (comma != std::string::npos) {
      try {
        double const freq_hz = std::stod(resp.substr(comma + 1));
        freq_mhz = freq_hz / 1000000.0;
        flush_serial();
        return true;
      } catch (...) {
        return false;
      }
    }
  }
  return false;
}

/**
 * @brief Set the radio frequency with TH-D75 CAT fallback
 *
 * Issues direct Kenwood FQ CAT command ("FQ <band>,<10-digit Hz>;\r").
 */
auto THD75::set_frequency(double freq_mhz) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  int active_band = (get_current_vfo() == VFO::B) ? 1 : 0;

  auto freq_hz = static_cast<long long>(freq_mhz * 1e6);
  char cmd[BUFFER_SIZE_32];
  snprintf(cmd, sizeof(cmd), "FQ %d,%010lld;\r", active_band, freq_hz);

  char buf[response_buffer_size] = {0};
  unsigned char term = '\r';
  flush_serial();
  int const bytes = rig_send_raw(
      rig_, reinterpret_cast<const unsigned char *>(cmd),
      static_cast<int>(strlen(cmd)), reinterpret_cast<unsigned char *>(buf),
      static_cast<int>(sizeof(buf)) - 1, &term);

  bool const success = is_valid_cat_response(bytes, buf);
  flush_serial();
  return success;
}
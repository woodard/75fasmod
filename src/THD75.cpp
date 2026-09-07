#include "THD75.hpp"
/**
 * @file THD75.hpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 *
 * This class implements the TH-D75 specific functionality for the
 * RadioController base class. It provides implementations using
 * Hamlib's Kenwood TH-D75 backend where needed.
 */

#include "hamlib/rig.h"
#include <cctype>
#include <cmath>
#include <iostream>
#include <thread>

THD75::THD75(std::string port, rig_model_t model, bool hamlib_debug)
    : RadioController(model, std::move(port), hamlib_debug) {}

THD75::~THD75() {
  if (rig_ != nullptr) {
    std::cerr << "[RIG] Cleaning up TH-D75 instance..." << std::endl;
    // Let base RadioController destructor handle clean rig_close()
    // without issuing trailing commands over the wire.
  }
}

auto THD75::set_power_level(const std::string &level) -> bool {
  if (rig_ == nullptr)
    return false;
  std::string lvl = level;
  for (auto &chr : lvl)
    chr = static_cast<char>(std::toupper(chr));
  PowerLevel val = PowerLevel::UNKNOWN;
  if (lvl == "H")
    val = PowerLevel::HIGH;
  else if (lvl == "M")
    val = PowerLevel::MEDIUM;
  else if (lvl == "L")
    val = PowerLevel::LOW;
  else if (lvl == "EL")
    val = PowerLevel::EXTRA_LOW;
  else
    return false;
  return kenwood_power_set(val);
}

auto THD75::get_power_level(std::string &level) -> bool {
  if (rig_ == nullptr)
    return false;
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
  if (rig_ == nullptr || get_single())
    return false;
  VFO const current = get_current_vfo();
  VFO const other = (current == VFO::A) ? VFO::B : VFO::A;
  if (!set_current_vfo(other))
    return false;
  bool const result = set_power_level(level);
  set_current_vfo(current);
  return result;
}

auto THD75::get_other_power_level(std::string &level) -> bool {
  if (rig_ == nullptr || get_single()) {
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
  return result;
}

auto THD75::initialize() -> bool {
  if (rig_ == nullptr)
    return false;
  std::cerr << "[RIG] Backing up current radio state..." << std::endl;

  if (get_dual()) {
    VFO const current = get_current_vfo();
    VFO const other = (current == VFO::A) ? VFO::B : VFO::A;
    if (set_current_vfo(other)) {
      double freq = 0.0;
      if (get_frequency(freq)) {
        orig_other_frequency_ = static_cast<freq_t>(freq * 1e6);
        orig_other_freq_saved_ = true;
      }
      Mode mode;
      if (get_mode(mode)) {
        orig_other_mode_ = static_cast<rmode_t>(mode);
        orig_other_mode_saved_ = true;
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
      }
      set_current_vfo(current);
    }
  }

  rig_get_vfo(rig_, &orig_vfo_);
  orig_power_ = kenwood_power_get();
  orig_power_saved_ = true;
  orig_menu_102_ = kenwood_usb_out_select_get();

  if (!RadioController::initialize())
    return false;

  rig_set_vfo(rig_, RIG_VFO_B);

  if (orig_menu_102_ != UsbOutSelect::unknown &&
      orig_menu_102_ != UsbOutSelect::IF) {
    kenwood_usb_out_select_set(UsbOutSelect::IF);

    // Radio reboots USB interface. Close handles first.
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;
    std::this_thread::sleep_for(std::chrono::seconds(4));

    rig_ = rig_init(model_);
    rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

    // Give the connection a moment to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (rig_open(rig_) != RIG_OK)
      return false;

    // MUST be called AFTER rig_open().
    // rig_open() spawns the background thread; this command kills it.
    rig_set_cache_timeout_ms(rig_, static_cast<hamlib_cache_t>(0), 0);

    // Wait for the background thread to safely exit, then flush
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    flush_serial();
  }

  if (rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, 9600) != RIG_OK) {
    rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_FM, 0);
  }
  return true;
}

void THD75::shutdown() {
  if (rig_ != nullptr) {
    std::cerr << "[RIG] Restoring THD75 state..." << std::endl;
    if (get_dual() && orig_other_freq_saved_) {
      VFO const current = get_current_vfo();
      VFO const other = (current == VFO::A) ? VFO::B : VFO::A;
      if (set_current_vfo(other)) {
        if (orig_other_freq_saved_)
          set_frequency(static_cast<double>(orig_other_frequency_) / 1e6);
        if (orig_other_mode_saved_)
          set_mode(static_cast<Mode>(orig_other_mode_));
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
          if (level != "UNKNOWN")
            set_power_level(level);
        }
        set_current_vfo(current);
      }
    }
    if (orig_menu_102_ != UsbOutSelect::unknown)
      kenwood_usb_out_select_set(orig_menu_102_);
    if (orig_vfo_ != RIG_VFO_NONE)
      rig_set_vfo(rig_, orig_vfo_);
  }
  RadioController::shutdown();
}

auto THD75::get_tnc() -> int {
  if (rig_ == nullptr)
    return -1;
  char cmd[] = "TN\r", buf[64] = {0};
  unsigned char term = '\r';
  int bytes =
      rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd), 3,
                   reinterpret_cast<unsigned char *>(buf), 63, &term);
  if (bytes > 0 && buf[0] != '?' && buf[0] != 'N') {
    std::string const resp(buf);
    size_t space_pos = resp.find(' ');
    size_t comma = resp.find(',');
    if (comma != std::string::npos && space_pos != std::string::npos) {
      try {
        return std::stoi(resp.substr(space_pos + 1, comma - (space_pos + 1)));
      } catch (...) {
        return -1;
      }
    }
  }
  return -1;
}

auto THD75::set_tnc(int mode) -> bool {
  if (rig_ == nullptr)
    return false;
  char cmd[32], buf[64] = {0};
  snprintf(cmd, sizeof(cmd), "TN %d,0\r", mode);
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           strlen(cmd), reinterpret_cast<unsigned char *>(buf),
                           63, &term);
  return bytes > 0 && buf[0] != '?' && buf[0] != 'N';
}

auto THD75::kenwood_menu_get(int menu_num) -> int {
  if (rig_ == nullptr)
    return -1;
  char cmd[16], buf[64] = {0};
  snprintf(cmd, sizeof(cmd), "EX%03d\r", menu_num);
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           strlen(cmd), reinterpret_cast<unsigned char *>(buf),
                           63, &term);
  if (bytes > 0 && buf[0] != '?' && buf[0] != 'N') {
    std::string resp(buf);
    size_t comma = resp.find(','), tpos = resp.find('\r');
    if (tpos == std::string::npos)
      tpos = resp.find(';');
    if (comma != std::string::npos && tpos != std::string::npos) {
      try {
        return std::stoi(resp.substr(comma + 1, tpos - comma - 1));
      } catch (...) {
      }
    }
  }
  return -1;
}

auto THD75::kenwood_menu_set(int menu_num, int value) -> bool {
  if (rig_ == nullptr || value < 0)
    return false;
  char cmd[32], buf[64] = {0};
  snprintf(cmd, sizeof(cmd), "EX%03d,%d\r", menu_num, value);
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           strlen(cmd), reinterpret_cast<unsigned char *>(buf),
                           63, &term);
  return bytes > 0 && buf[0] != '?' && buf[0] != 'N';
}

auto THD75::kenwood_usb_out_select_get() -> THD75::UsbOutSelect {
  int val = kenwood_menu_get(102);
  if (val == 0)
    return UsbOutSelect::AF;
  if (val == 1)
    return UsbOutSelect::IF;
  if (val == 2)
    return UsbOutSelect::Detect;
  return UsbOutSelect::unknown;
}

auto THD75::kenwood_usb_out_select_set(UsbOutSelect value) -> bool {
  return kenwood_menu_set(102, static_cast<int>(value));
}

auto THD75::kenwood_power_get() -> THD75::PowerLevel {
  if (rig_ == nullptr)
    return PowerLevel::UNKNOWN;
  int active_band = (get_current_vfo() == VFO::B) ? 1 : 0;
  char cmd[16], buf[64] = {0};
  snprintf(cmd, sizeof(cmd), "PC %d\r", active_band);
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           strlen(cmd), reinterpret_cast<unsigned char *>(buf),
                           63, &term);
  if (bytes > 0 && buf[0] != '?' && buf[0] != 'N') {
    std::string resp(buf);
    size_t comma = resp.find(','), tpos = resp.find('\r');
    if (tpos == std::string::npos)
      tpos = resp.find(';');
    if (comma != std::string::npos && tpos != std::string::npos) {
      try {
        return static_cast<PowerLevel>(
            std::stoi(resp.substr(comma + 1, tpos - comma - 1)));
      } catch (...) {
      }
    }
  }
  return PowerLevel::UNKNOWN;
}

auto THD75::kenwood_power_set(PowerLevel val) -> bool {
  if (rig_ == nullptr || val == PowerLevel::UNKNOWN)
    return false;
  int active_band = (get_current_vfo() == VFO::B) ? 1 : 0;
  char cmd[32], buf[64] = {0};
  snprintf(cmd, sizeof(cmd), "PC %d,%d\r", active_band, static_cast<int>(val));
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           strlen(cmd), reinterpret_cast<unsigned char *>(buf),
                           63, &term);
  return bytes > 0 && buf[0] != '?' && buf[0] != 'N';
}

auto THD75::get_current_vfo() -> VFO {
  if (rig_ == nullptr)
    return VFO::A;
  vfo_t vfo = 0;
  if (rig_get_vfo(rig_, &vfo) == RIG_OK) {
    if (vfo == RIG_VFO_B)
      return VFO::B;
  }
  return VFO::A;
}

auto THD75::set_current_vfo(VFO vfo) -> bool {
  if (rig_ == nullptr)
    return false;
  return rig_set_vfo(rig_, (vfo == VFO::A) ? RIG_VFO_A : RIG_VFO_B) == RIG_OK;
}

auto THD75::get_single() -> bool {
  if (rig_ == nullptr)
    return false;
  int status = 0;
  if (rig_get_func(rig_, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, &status) ==
      RIG_OK) {
    return status == 0;
  }
  return true;
}

auto THD75::get_dual() -> bool {
  if (rig_ == nullptr)
    return false;
  int status = 0;
  if (rig_get_func(rig_, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, &status) ==
      RIG_OK) {
    return status != 0;
  }
  return false;
}

auto THD75::set_single(VFO vfo) -> bool {
  if (rig_ == nullptr)
    return false;
  if (!set_current_vfo(vfo))
    return false;
  return rig_set_func(rig_, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, 0) == RIG_OK;
}

auto THD75::set_dual() -> bool {
  if (rig_ == nullptr)
    return false;
  return rig_set_func(rig_, RIG_VFO_CURR, RIG_FUNC_DUAL_WATCH, 1) == RIG_OK;
}

auto THD75::flip_single_dual(VFO vfo) -> bool {
  if (rig_ == nullptr)
    return false;
  if (get_single())
    return set_dual();
  return set_single(vfo);
}

auto THD75::set_other_mode(Mode mode) -> bool {
  if (rig_ == nullptr || get_single())
    return false;
  VFO current = get_current_vfo();
  VFO other = (current == VFO::A) ? VFO::B : VFO::A;
  if (!set_current_vfo(other))
    return false;
  bool result = set_mode(mode);
  set_current_vfo(current);
  return result;
}

auto THD75::get_other_mode() -> Mode {
  if (rig_ == nullptr || get_single())
    return Mode::FM;
  VFO current = get_current_vfo();
  VFO other = (current == VFO::A) ? VFO::B : VFO::A;
  if (!set_current_vfo(other))
    return Mode::FM;
  Mode mode;
  bool result = get_mode(mode);
  set_current_vfo(current);
  if (!result)
    return Mode::FM;
  return mode;
}

auto THD75::set_other_frequency(double freq_mhz) -> bool {
  if (rig_ == nullptr || get_single())
    return false;
  VFO current = get_current_vfo();
  VFO other = (current == VFO::A) ? VFO::B : VFO::A;
  if (!set_current_vfo(other))
    return false;
  bool result = set_frequency(freq_mhz);
  set_current_vfo(current);
  return result;
}

auto THD75::get_other_frequency() -> double {
  if (rig_ == nullptr || get_single())
    return 0.0;
  VFO current = get_current_vfo();
  VFO other = (current == VFO::A) ? VFO::B : VFO::A;
  if (!set_current_vfo(other))
    return 0.0;
  double freq = NAN;
  bool result = get_frequency(freq);
  set_current_vfo(current);
  if (!result)
    return 0.0;
  return freq;
}

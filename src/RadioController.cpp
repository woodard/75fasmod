#include "RadioController.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

std::string RadioController::read_sysfs_attr(const fs::path &filepath) {
  std::ifstream file(filepath);
  std::string value;
  if (file >> value)
    return value;
  return "";
}

RadioController::RadioController(rig_model_t model, const std::string &port,
                                 bool hamlib_debug)
    : model_(model), port_(port), rig_(nullptr), orig_mode_(RIG_MODE_NONE),
      orig_mode_saved_(false), orig_vfo_(RIG_VFO_NONE), orig_width_(0),
      orig_power_(PowerLevel::UNKNOWN), orig_menu_102_(UsbOutSelect::unknown),
      orig_tnc_state_(-1) {
  // Enable Hamlib internal verbose trace logging only if requested
  if (hamlib_debug) {
    rig_set_debug_level(RIG_DEBUG_TRACE);
  }

  std::cout << "[RIG] Initializing Hamlib model ID " << model_ << "...\n";
  rig_ = rig_init(model_);
  if (!rig_) {
    std::cerr << "[RIG] Error: rig_init() failed for model ID " << model_
              << ". The model ID may not exist in this Hamlib build.\n";
    return;
  }

  rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

  int status = rig_open(rig_);
  if (status != RIG_OK) {
    std::cerr << "[RIG] Error: rig_open() failed on " << port_
              << " | Code: " << status << " (" << rigerror(status) << ")\n";
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;
    return;
  }
}
RadioController::~RadioController() {
  if (rig_) {
    std::cout << "[RIG] Shutting down. Restoring original radio settings...\n";
    set_ptt(false);

    // Restore original menu 102 (only if we successfully queried it)
    if (orig_menu_102_ != UsbOutSelect::unknown) {
      if (!kenwood_usb_out_select_set(orig_menu_102_)) {
        std::cerr
            << "[RIG] Warning: Failed to restore original Menu 102 setting.\n";
      }
    }

    // Restore original VFO
    if (orig_vfo_ != RIG_VFO_NONE) {
      if (rig_set_vfo(rig_, orig_vfo_) == RIG_OK) {
        std::cout << "[RIG] Restored VFO to " << rig_strvfo(orig_vfo_) << "\n";
      }
    }

    // Restore original TNC state
    if (orig_tnc_state_ != -1) {
      std::cout << "[RIG] Restoring TNC state to " << orig_tnc_state_
                << "...\n";
      if (!kenwood_tnc_set(orig_tnc_state_)) {
        std::cerr << "[RIG] Warning: Failed to restore original TNC state.\n";
      }
    }

    if (orig_power_ != PowerLevel::UNKNOWN) {
      if (!kenwood_power_set(orig_power_)) {
        std::cerr << "[RIG] Warning: Failed to restore original TX power.\n";
      }
    }

    // Restore original operating mode and bandwidth
    if (orig_mode_ != RIG_MODE_NONE) {
      std::cout << "[RIG] Restoring original mode (" << rig_strrmode(orig_mode_)
                << ")...\n";
      rig_set_mode(rig_, RIG_VFO_CURR, orig_mode_, orig_width_);
    }

    rig_close(rig_);
    rig_cleanup(rig_);
  }
}

bool RadioController::initialize() {
  std::cout << "[RIG] Backing up current radio state...\n";

  // 1. Save original operating mode and bandwidth
  if (rig_get_mode(rig_, RIG_VFO_CURR, &orig_mode_, &orig_width_) == RIG_OK) {
    std::cout << "[RIG] Saved original mode: " << rig_strrmode(orig_mode_)
              << "\n";
  } else {
    std::cerr << "[RIG] Warning: Could not query starting radio mode.\n";
  }

  // 2. Save current VFO state
  if (rig_get_vfo(rig_, &orig_vfo_) == RIG_OK) {
    std::cout << "[RIG] Saved VFO state: " << rig_strvfo(orig_vfo_) << "\n";
  }

  // 3. Save TNC state before any modifications
  orig_tnc_state_ = kenwood_tnc_get();
  std::cout << "[RIG] Saved TNC state: " << orig_tnc_state_ << "\n";

  // Turn off TNC if it's on to allow dual mode changes
  if (orig_tnc_state_ != 0 && orig_tnc_state_ != -1) {
    std::cout << "[RIG] Turning off TNC to allow dual mode changes...\n";
    if (!kenwood_tnc_set(0)) {
      std::cerr << "[RIG] CRITICAL ERROR: Could not turn off TNC.\n";
      return false;
    }
  }

  // Set to VFO B to allow menu 102 changes when in dual mode
  std::cout << "[RIG] Setting radio to VFO B for menu 102 access...\n";
  rig_set_vfo(rig_, RIG_VFO_B);

  // 4. Query power and Menu 102 state
  orig_power_ = kenwood_power_get();
  orig_menu_102_ = kenwood_usb_out_select_get();

  if (orig_menu_102_ == UsbOutSelect::unknown) {
    std::cerr
        << "\n[RIG] WARNING: Could not query Menu 102 via CAT.\n"
        << "      Kenwood locks this menu over USB. Please manually ensure\n"
        << "      Menu 102 (USB Out Select) is set to IF Output (1).\n\n";
    // We purposefully DO NOT return false here. We bypass the error and
    // continue.
  }

  std::cout << "[RIG] Configuring radio for high-speed modem operation...\n";

  // 5. Set mode to Packet FM (9600 baud passband)
  int mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, 9600);
  if (mode_ret != RIG_OK) {
    std::cout << "[RIG] PKTFM mode rejected, falling back to standard FM...\n";
    mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_FM, 0);
  }

  // Verify radio is in an FM mode
  rmode_t active_mode = RIG_MODE_NONE;
  pbwidth_t active_width = 0;
  if (rig_get_mode(rig_, RIG_VFO_CURR, &active_mode, &active_width) == RIG_OK) {
    if (active_mode != RIG_MODE_PKTFM && active_mode != RIG_MODE_FM) {
      std::cerr
          << "[RIG] WARNING: Radio failed to enter FM mode! Current mode: "
          << rig_strrmode(active_mode) << "\n";
    } else {
      std::cout << "[RIG] Verified active mode: " << rig_strrmode(active_mode)
                << "\n";
    }
  }

  // 6. Configure Kenwood 9600 bps data output path (Menu 102) safely
  if (orig_menu_102_ != UsbOutSelect::unknown &&
      orig_menu_102_ != UsbOutSelect::IF) {
    std::cout << "[RIG] Changing Menu 102 to IF Output (1). This will cause a "
                 "USB reset...\n";
    if (!kenwood_usb_out_select_set(UsbOutSelect::IF)) {
      std::cerr
          << "[RIG] CRITICAL ERROR: Could not switch Menu 102 to IF output.\n";
      return false;
    }

    // The radio is currently rebooting its USB interface.
    // Close our stale handles before the OS gets upset.
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;

    std::cout << "[RIG] Waiting 4 seconds for USB re-enumeration...\n";
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // Re-initialize Hamlib now that the radio has returned
    std::cout << "[RIG] Reconnecting to Hamlib after USB reset...\n";
    rig_ = rig_init(model_);
    rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

    int re_status = rig_open(rig_);
    if (re_status != RIG_OK) {
      std::cerr << "[RIG] Error: Failed to reconnect after USB reset. Code: "
                << re_status << "\n";
      return false;
    }
    std::cout << "[RIG] Successfully reconnected to radio.\n";
  } else if (orig_menu_102_ == UsbOutSelect::IF) {
    std::cout
        << "[RIG] Menu 102 already set to IF Output. Skipping USB reset.\n";
  }

  return true;
}

bool RadioController::set_frequency(double freq_mhz) {
  freq_t freq_hz = static_cast<freq_t>(freq_mhz * 1000000.0);
  return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

bool RadioController::get_frequency(double &freq_mhz) {
  freq_t freq_hz;
  if (rig_get_freq(rig_, RIG_VFO_CURR, &freq_hz) != RIG_OK) {
    return false;
  }
  freq_mhz = static_cast<double>(freq_hz) / 1000000.0;
  return true;
}

bool RadioController::set_ptt(bool transmit) {
  const char *cmd = transmit ? "TX\r" : "RX\r";
  char buf[64] = {0};
  unsigned char term = '\r';

  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes < 0 && bytes != -RIG_ETIMEOUT && bytes != RIG_ETIMEOUT) {
    return false;
  }
  return true;
}

bool RadioController::get_dcd(bool &is_squelch_open) {
  dcd_t dcd_status;
  if (rig_get_dcd(rig_, RIG_VFO_CURR, &dcd_status) == RIG_OK) {
    is_squelch_open = (dcd_status == RIG_DCD_ON);
    return true;
  }
  return false;
}

bool RadioController::set_power_level(const std::string &level) {
  std::string lvl = level;
  for (auto &c : lvl)
    c = std::toupper(c);

  PowerLevel val = PowerLevel::UNKNOWN;
  if (lvl == "H")
    val = PowerLevel::HIGH;
  else if (lvl == "M")
    val = PowerLevel::MEDIUM;
  else if (lvl == "L")
    val = PowerLevel::LOW;
  else if (lvl == "EL")
    val = PowerLevel::EXTRA_LOW;
  else {
    std::cerr << "Error: Invalid power level '" << level
              << "'. Use EL, L, M, or H.\n";
    return false;
  }

  std::cout << "[RIG] Setting TX power to " << lvl << "...\n";
  return kenwood_power_set(val);
}

bool RadioController::get_power_level(std::string &level) {
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

int RadioController::kenwood_menu_get(int menu_num) {
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "EX%03d\r",
           menu_num); // Standard Kenwood EX command

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes < 0) {
    std::cerr << "[RIG] Transport error querying Menu " << menu_num << " ("
              << rigerror(bytes) << ")\n";
    return -1;
  }

  if (bytes > 0 && (buf[0] == '?' ||
                    (buf[0] == 'E' && (buf[1] == '\r' || buf[1] == '\0')))) {
    // Suppress the giant error output since we now expect Menu 102 to throw a ?
    return -1;
  }

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

bool RadioController::kenwood_menu_set(int menu_num, int value) {
  if (value < 0)
    return false;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "EX%03d,%d\r", menu_num, value);

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes < 0 && bytes != -RIG_ETIMEOUT && bytes != RIG_ETIMEOUT) {
    std::cerr << "[RIG] Transport error configuring Menu " << menu_num << " ("
              << rigerror(bytes) << ")\n";
    return false;
  }

  if (bytes > 0 && (buf[0] == '?' ||
                    (buf[0] == 'E' && (buf[1] == '\r' || buf[1] == '\0')))) {
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int actual = kenwood_menu_get(menu_num);
  if (actual != value) {
    return false;
  }

  return true;
}

RadioController::PowerLevel RadioController::kenwood_power_get() {
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

  if (bytes < 0) {
    std::cerr << "[RIG] Transport error querying power level ("
              << rigerror(bytes) << ")\n";
    return PowerLevel::UNKNOWN;
  }

  if (bytes > 0 && (buf[0] == '?' ||
                    (buf[0] == 'E' && (buf[1] == '\r' || buf[1] == '\0')))) {
    std::cerr << "[RIG] Firmware error querying power: " << buf << "\n";
    return PowerLevel::UNKNOWN;
  }

  if (bytes > 0) {
    std::string resp(buf);
    size_t comma = resp.find(',');
    size_t term_pos = resp.find('\r');
    if (term_pos == std::string::npos)
      term_pos = resp.find(';');

    // PC response looks like "PC 0,3\r" (Band, Power)
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

bool RadioController::kenwood_power_set(PowerLevel val) {
  if (val == PowerLevel::UNKNOWN)
    return false;

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

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "PC %d,%d\r", active_band, static_cast<int>(val));

  char buf[64] = {0};
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes < 0 && bytes != -RIG_ETIMEOUT && bytes != RIG_ETIMEOUT) {
    std::cerr << "[RIG] Transport error setting power level ("
              << rigerror(bytes) << ")\n";
    return false;
  }

  if (bytes > 0 && (buf[0] == '?' ||
                    (buf[0] == 'E' && (buf[1] == '\r' || buf[1] == '\0')))) {
    std::cerr << "[RIG] Firmware error setting power: " << buf << "\n";
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  PowerLevel actual = kenwood_power_get();
  if (actual != val) {
    std::cerr << "[RIG] Verification failed for Power Level. Expected "
              << static_cast<int>(val) << " but got "
              << static_cast<int>(actual) << ".\n";
    return false;
  }

  return true;
}

int RadioController::kenwood_tnc_get() {
  char cmd[] = "TN\r";
  char buf[64] = {0};
  unsigned char term = '\r';

  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes < 0) {
    std::cerr << "[RIG] Transport error querying TNC state (" << rigerror(bytes)
              << ")\n";
    return -1;
  }

  if (bytes > 0 && (buf[0] == '?' ||
                    (buf[0] == 'E' && (buf[1] == '\r' || buf[1] == '\0')))) {
    std::cerr << "[RIG] Firmware error querying TNC state: " << buf << "\n";
    return -1;
  }

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

bool RadioController::kenwood_tnc_set(int mode) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "TN %d,0\r", mode);

  char buf[64] = {0};
  unsigned char term = '\r';
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes < 0 && bytes != -RIG_ETIMEOUT && bytes != RIG_ETIMEOUT) {
    std::cerr << "[RIG] Transport error setting TNC state (" << rigerror(bytes)
              << ")\n";
    return false;
  }

  if (bytes > 0 && (buf[0] == '?' ||
                    (buf[0] == 'E' && (buf[1] == '\r' || buf[1] == '\0')))) {
    std::cerr << "[RIG] Firmware error setting TNC state: " << buf << "\n";
    return false;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  int actual = kenwood_tnc_get();
  if (actual != mode) {
    std::cerr << "[RIG] Verification failed for TNC Mode. Expected " << mode
              << " but got " << actual << ".\n";
    return false;
  }

  return true;
}

RadioController::UsbOutSelect RadioController::kenwood_usb_out_select_get() {
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

bool RadioController::kenwood_usb_out_select_set(UsbOutSelect value) {
  return kenwood_menu_set(102, static_cast<int>(value));
}

std::vector<std::string>
RadioController::find_tty_sysfs(unsigned int target_vid,
                                unsigned int target_pid) {
  std::vector<std::string> found_ports;
  fs::path sys_tty = "/sys/class/tty";

  if (!fs::exists(sys_tty))
    return found_ports;

  for (const auto &entry : fs::directory_iterator(sys_tty)) {
    fs::path dev_path = entry.path() / "device";
    if (!fs::exists(dev_path))
      continue;

    for (const char *parent_rel : {"..", "../..", "../../.."}) {
      fs::path vid_path = dev_path / parent_rel / "idVendor";
      fs::path pid_path = dev_path / parent_rel / "idProduct";

      if (fs::exists(vid_path) && fs::exists(pid_path)) {
        unsigned int vid = 0, pid = 0;
        if (auto vid_str = read_sysfs_attr(vid_path); !vid_str.empty()) {
          vid = std::stoul(vid_str, nullptr, 16);
        }
        if (auto pid_str = read_sysfs_attr(pid_path); !pid_str.empty()) {
          pid = std::stoul(pid_str, nullptr, 16);
        }

        if (vid == target_vid && pid == target_pid) {
          found_ports.push_back("/dev/" + entry.path().filename().string());
          break;
        }
      }
    }
  }
  return found_ports;
}

std::string RadioController::find_alsa_device(const std::string &serial_port) {
  fs::path tty_name = fs::path(serial_port).filename();
  fs::path tty_dev_path = "/sys/class/tty" / tty_name / "device";

  if (!fs::exists(tty_dev_path))
    return "";

  fs::path usb_dev_path;
  try {
    usb_dev_path = fs::canonical(tty_dev_path).parent_path();
  } catch (...) {
    return "";
  }

  fs::path sound_class_path = "/sys/class/sound";
  if (!fs::exists(sound_class_path))
    return "";

  for (const auto &entry : fs::directory_iterator(sound_class_path)) {
    std::string card_name = entry.path().filename().string();

    if (card_name.find("card") == 0) {
      fs::path card_dev_path = entry.path() / "device";
      if (!fs::exists(card_dev_path))
        continue;

      try {
        fs::path card_usb_path = fs::canonical(card_dev_path).parent_path();
        if (card_usb_path == usb_dev_path) {
          std::string card_num = card_name.substr(4);
          return "hw:" + card_num + ",0";
        }
      } catch (...) {
        continue;
      }
    }
  }
  return "";
}
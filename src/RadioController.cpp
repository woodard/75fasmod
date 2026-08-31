#include "RadioController.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <fstream>

static std::string read_sysfs_attr(const fs::path& filepath) {
  std::ifstream file(filepath);
  std::string value;
  if (file >> value) return value;
  return "";
}

RadioController::RadioController(rig_model_t model, const std::string &port)
    : model_(model), port_(port), rig_(nullptr), orig_mode_(RIG_MODE_NONE),
      orig_width_(0), orig_menu_102_(UsbOutSelect::unknown), orig_power_(PowerLevel::UNKNOWN),
      orig_tnc_state_(-1), orig_vfo_(RIG_VFO_NONE) {}

RadioController::~RadioController() {
  if (rig_) {
    std::cout << "[RIG] Shutting down. Restoring original radio settings...\n";
    set_ptt(false);

    // Restore original menu 102
    kenwood_usb_out_select_set(orig_menu_102_);

    // Restore original VFO
    if (rig_set_vfo(rig_, orig_vfo_) == RIG_OK) {
      std::cout << "[RIG] Restored VFO to " << rig_strvfo(orig_vfo_) << "\n";
    }

    // Restore original TNC state
    if (orig_tnc_state_ != -1) {
      std::cout << "[RIG] Restoring TNC state to " << orig_tnc_state_ << "...\n";
      kenwood_tnc_set(orig_tnc_state_);
    }

    // Restore original operating mode and bandwidth
    if (orig_mode_ != RIG_MODE_NONE) {
      std::cout << "[RIG] Restoring original mode (" << rig_strrmode(orig_mode_) << ")...\n";
      rig_set_mode(rig_, RIG_VFO_CURR, orig_mode_, orig_width_);
    }

    rig_close(rig_);
    rig_cleanup(rig_);
  }
}

bool RadioController::initialize(bool hamlib_debug) {
  // Enable Hamlib internal verbose trace logging only if requested
  if (hamlib_debug) {
    rig_set_debug_level(RIG_DEBUG_TRACE);
  }

  std::cout << "[RIG] Initializing Hamlib model ID " << model_ << "...\n";
  rig_ = rig_init(model_);
  if (!rig_) {
    std::cerr << "[RIG] Error: rig_init() failed for model ID " << model_ 
              << ". The model ID may not exist in this Hamlib build.\n";
    return false;
  }

  rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

  int status = rig_open(rig_);
  if (status != RIG_OK) {
    std::cerr << "[RIG] Error: rig_open() failed on " << port_ 
              << " | Code: " << status << " (" << rigerror(status) << ")\n";
    return false;
  }

  std::cout << "[RIG] Backing up current radio state...\n";

  // 1. Save original operating mode and bandwidth
  if (rig_get_mode(rig_, RIG_VFO_CURR, &orig_mode_, &orig_width_) == RIG_OK) {
    std::cout << "[RIG] Saved original mode: " << rig_strrmode(orig_mode_) << "\n";
  } else {
    std::cerr << "[RIG] Warning: Could not query starting radio mode.\n";
  }

  orig_menu_102_ = kenwood_usb_out_select_get();
  orig_power_ = kenwood_power_get();

  // Save TNC state before any modifications
  orig_tnc_state_ = kenwood_tnc_get();
  std::cout << "[RIG] Saved TNC state: " << orig_tnc_state_ << "\n";

  // Turn off TNC if it's on to allow dual mode changes
  if (orig_tnc_state_ != 0) {
    std::cout << "[RIG] Turning off TNC to allow dual mode changes...\n";
    kenwood_tnc_set(0);
  }

  // Save current VFO state
  if (rig_get_vfo(rig_, &orig_vfo_) == RIG_OK) {
    std::cout << "[RIG] Saved VFO state: " << rig_strvfo(orig_vfo_) << "\n";
  }

  // Set to VFO B to allow menu 102 changes when in dual mode
  std::cout << "[RIG] Setting radio to VFO B for menu 102 access...\n";
  rig_set_vfo(rig_, RIG_VFO_B);

  std::cout << "[RIG] Configuring radio for high-speed modem operation...\n";

  // 2. Set mode to Packet FM (9600 baud passband)
  int mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, 9600);
  if (mode_ret != RIG_OK) {
    std::cout << "[RIG] PKTFM mode rejected, falling back to standard FM...\n";
    mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_FM, 0);
  }

  // 3. Verify radio is in an FM mode
  rmode_t active_mode = RIG_MODE_NONE;
  pbwidth_t active_width = 0;
  if (rig_get_mode(rig_, RIG_VFO_CURR, &active_mode, &active_width) == RIG_OK) {
    if (active_mode != RIG_MODE_PKTFM && active_mode != RIG_MODE_FM) {
      std::cerr << "[RIG] WARNING: Radio failed to enter FM mode! Current mode: "
                << rig_strrmode(active_mode) << "\n";
    } else {
      std::cout << "[RIG] Verified active mode: " << rig_strrmode(active_mode) << "\n";
    }
  }

  // 4. Configure Kenwood 9600 bps data output path (Menu 102) safely
  if (orig_menu_102_ != UsbOutSelect::IF) {
    std::cout << "[RIG] Changing Menu 102 to IF Output (1). This will cause a USB reset...\n";
    kenwood_usb_out_select_set(UsbOutSelect::IF);
    
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
  } else {
    std::cout << "[RIG] Menu 102 already set to IF Output. Skipping USB reset.\n";
  }

  return true;
}

bool RadioController::set_frequency(double freq_mhz) {
  freq_t freq_hz = static_cast<freq_t>(freq_mhz * 1000000.0);
  return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

bool RadioController::set_ptt(bool transmit) {
  ptt_t ptt_state = transmit ? RIG_PTT_ON : RIG_PTT_OFF;
  return rig_set_ptt(rig_, RIG_VFO_CURR, ptt_state) == RIG_OK;
}

bool RadioController::get_dcd(bool &is_squelch_open) {
  dcd_t dcd_status;
  if (rig_get_dcd(rig_, RIG_VFO_CURR, &dcd_status) == RIG_OK) {
    is_squelch_open = (dcd_status == RIG_DCD_ON);
    return true;
  }
  return false;
}

int RadioController::kenwood_menu_get(int menu_num) {
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "EX%03d;", menu_num);

  char buf[64] = {0};
  unsigned char term = ';';

  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    size_t comma = resp.find(',');
    size_t semi = resp.find(';');
    if (comma != std::string::npos && semi != std::string::npos) {
      try {
        return std::stoi(resp.substr(comma + 1, semi - comma - 1));
      } catch (...) {
        return -1;
      }
    }
  }
  return -1;
}

void RadioController::kenwood_menu_set(int menu_num, int value) {
  if (value < 0)
    return;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "EX%03d,%d;", menu_num, value);

  rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd), nullptr, 0,
               nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool RadioController::set_power_level(const std::string &level) {
  std::string lvl = level;
  for (auto &c : lvl)
    c = std::toupper(c);

  float pwr_float = 1.0f; // Default High (5W)
  PowerLevel val = PowerLevel::UNKNOWN;

  if (lvl == "H") {
    pwr_float = 1.0f;
    val = PowerLevel::HIGH;
  } else if (lvl == "M") {
    pwr_float = 0.4f;   // Mid (~2W)
    val = PowerLevel::MEDIUM;
  } else if (lvl == "L") {
    pwr_float = 0.1f;   // Low (~0.5W)
    val = PowerLevel::LOW;
  } else if (lvl == "EL") {
    pwr_float = 0.01f;  // Extra Low (~0.05W)
    val = PowerLevel::EXTRA_LOW;
  } else {
    std::cerr << "Error: Invalid power level '" << level
              << "'. Use EL, L, M, or H.\n";
    return false;
  }

  // 1. Query the currently active VFO/Band from Hamlib
  vfo_t active_vfo = RIG_VFO_CURR;
  if (rig_get_vfo(rig_, &active_vfo) == RIG_OK) {
    std::cout << "[RIG] Active VFO detected: " << rig_strvfo(active_vfo) << "\n";
  } else {
    active_vfo = RIG_VFO_CURR;
  }

  // 2. Wrap the float in Hamlib's value_t union
  value_t pwr_val{};
  pwr_val.f = pwr_float;

  // 3. Attempt Hamlib Native RF Power setting on active VFO
  std::cout << "[RIG] Setting TX power to " << lvl << " on " << rig_strvfo(active_vfo) << "...\n";
  int status = rig_set_level(rig_, active_vfo, RIG_LEVEL_RFPOWER, pwr_val);

  if (status != RIG_OK) {
    std::cerr << "[RIG] Warning: rig_set_level failed (" << status << "). Falling back to raw CAT...\n";
    kenwood_power_set(val);
  }

  return true;
}

RadioController::PowerLevel RadioController::kenwood_power_get() {
  // Determine active band (0 = Band A, 1 = Band B) via BC command
  int active_band = 0;
  char bc_buf[32] = {0};
  unsigned char term = ';';

  if (rig_send_raw(rig_, (const unsigned char *)"BC;", 3,
                           (unsigned char *)bc_buf, sizeof(bc_buf) - 1, &term) > 0) {
    std::string bc_resp(bc_buf);
    if (bc_resp.find("BC 1") != std::string::npos) {
      active_band = 1;
    }
  }

  // Query power level for active band
  char cmd[16];
  snprintf(cmd, sizeof(cmd), "PC %d;", active_band);

  char buf[32] = {0};
  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    size_t comma = resp.find(',');
    size_t semi = resp.find(';');

    if (comma != std::string::npos && semi != std::string::npos && semi > comma + 1) {
      try {
        int pwr_int = std::stoi(resp.substr(comma + 1, semi - comma - 1));
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

void RadioController::kenwood_power_set(PowerLevel val) {
  if (val == PowerLevel::UNKNOWN)
    return;

  // Query active band (0 = Band A, 1 = Band B)
  int active_band = 0;
  char bc_buf[32] = {0};
  unsigned char term = ';';

  if (rig_send_raw(rig_, (const unsigned char *)"BC;", 3,
                           (unsigned char *)bc_buf, sizeof(bc_buf) - 1, &term) > 0) {
    std::string bc_resp(bc_buf);
    if (bc_resp.find("BC 1") != std::string::npos) {
      active_band = 1;
    }
  }

  // Send formatted power command: PC <band>,<level>;
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "PC %d,%d;", active_band, static_cast<int>(val));

  rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd), nullptr, 0, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

int RadioController::kenwood_tnc_get() {
  char cmd[] = "TNC;";
  char buf[32] = {0};
  unsigned char term = ';';

  int bytes = rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd),
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    // Response format: "TNC x,y" where x is mode, y is band
    if (resp.substr(0, 4) == "TNC ") {
      try {
        return std::stoi(resp.substr(4, 1));
      } catch (...) {
        return -1;
      }
    }
  }
  return -1;
}

void RadioController::kenwood_tnc_set(int mode) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "TNC %d;", mode);
  rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd), nullptr, 0, nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

RadioController::UsbOutSelect RadioController::kenwood_usb_out_select_get() {
  int value = kenwood_menu_get(102);
  switch (value) {
    case 0: return UsbOutSelect::AF;
    case 1: return UsbOutSelect::IF;
    case 2: return UsbOutSelect::Detect;
    default: return UsbOutSelect::unknown;
  }
}

void RadioController::kenwood_usb_out_select_set(UsbOutSelect value) {
  kenwood_menu_set(102, static_cast<int>(value));
}

std::vector<std::string> RadioController::find_tty_sysfs(unsigned int target_vid,
                                                       unsigned int target_pid) {
  std::vector<std::string> found_ports;
  fs::path sys_tty = "/sys/class/tty";
  
  if (!fs::exists(sys_tty)) return found_ports;

  for (const auto& entry : fs::directory_iterator(sys_tty)) {
    fs::path dev_path = entry.path() / "device";
    if (!fs::exists(dev_path)) continue;

    // Use const char* to avoid allocating temporary std::string objects
    for (const char* parent_rel : {"..", "../..", "../../.."}) {
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

std::string RadioController::find_alsa_device(const std::string& serial_port) {
  fs::path tty_name = fs::path(serial_port).filename(); // e.g., "ttyACM0"
  fs::path tty_dev_path = "/sys/class/tty" / tty_name / "device";

  if (!fs::exists(tty_dev_path)) return "";

  fs::path usb_dev_path;
  try {
    // The device node is a symlink to the USB interface. Its parent is the physical USB device.
    usb_dev_path = fs::canonical(tty_dev_path).parent_path();
  } catch (...) {
    return "";
  }

  fs::path sound_class_path = "/sys/class/sound";
  if (!fs::exists(sound_class_path)) return "";

  // Find the soundcard with the same parent USB device
  for (const auto& entry : fs::directory_iterator(sound_class_path)) {
    std::string card_name = entry.path().filename().string();
    
    if (card_name.find("card") == 0) {
      fs::path card_dev_path = entry.path() / "device";
      if (!fs::exists(card_dev_path)) continue;

      try {
        fs::path card_usb_path = fs::canonical(card_dev_path).parent_path();
        if (card_usb_path == usb_dev_path) {
          // Extract the X from "cardX" to format the ALSA hardware string
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

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
      orig_width_(0), orig_menu_102_(-1), orig_power_(PowerLevel::UNKNOWN) {}

RadioController::~RadioController() {
  if (rig_) {
    std::cout << "[RIG] Shutting down. Restoring original radio settings...\n";
    set_ptt(false);

    if (orig_menu_102_ >= 0) {
      kenwood_menu_set(102, orig_menu_102_);
    }

    if (orig_power_ != PowerLevel::UNKNOWN) {
      kenwood_power_set(orig_power_);
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

bool RadioController::initialize() {
  rig_ = rig_init(model_);
  if (!rig_)
    return false;

  rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

  if (rig_open(rig_) != RIG_OK) {
    std::cerr << "Error: Could not open radio on " << port_ << "\n";
    return false;
  }

  std::cout << "[RIG] Backing up current radio state...\n";

  // 1. Save original operating mode and bandwidth
  if (rig_get_mode(rig_, RIG_VFO_CURR, &orig_mode_, &orig_width_) == RIG_OK) {
    std::cout << "[RIG] Saved original mode: " << rig_strrmode(orig_mode_) << "\n";
  } else {
    std::cerr << "[RIG] Warning: Could not query starting radio mode.\n";
  }

  orig_menu_102_ = kenwood_menu_get(102);
  orig_power_ = kenwood_power_get();

  std::cout << "[RIG] Configuring radio for high-speed modem operation...\n";

  // 2. Set mode to Packet FM (9600 baud passband)
  int mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, 9600);
  if (mode_ret != RIG_OK) {
    std::cout << "[RIG] PKTFM mode rejected, falling back to standard FM...\n";
    mode_ret = rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_FM, 0);
  }

  // 3. Verify radio is in an FM mode and not AM, SSB, CW, or D-Star DV
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

  // Configure Kenwood 9600 bps data output path (Menu 102)
  kenwood_menu_set(102, 1);

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
  kenwood_power_set(val);
  return true;
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

RadioController::PowerLevel RadioController::kenwood_power_get() {
  char buf[32] = {0};
  unsigned char term = ';';

  int bytes = rig_send_raw(rig_, (const unsigned char *)"PC;", 3,
                           (unsigned char *)buf, sizeof(buf) - 1, &term);

  if (bytes > 0) {
    std::string resp(buf);
    size_t pc_pos = resp.find("PC");
    size_t semi = resp.find(';');

    if (pc_pos != std::string::npos && semi != std::string::npos &&
        semi > pc_pos + 2) {
      try {
        int pwr_int = std::stoi(resp.substr(pc_pos + 2, semi - pc_pos - 2));
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

  char cmd[16];
  snprintf(cmd, sizeof(cmd), "PC%d;", static_cast<int>(val));

  rig_send_raw(rig_, (const unsigned char *)cmd, strlen(cmd), nullptr, 0,
               nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
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


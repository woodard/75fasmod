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
      orig_mode_saved_(false), orig_width_(0),
      orig_power_(PowerLevel::UNKNOWN) {
  // Enable Hamlib internal verbose trace logging only if requested
  // Redirect Hamlib debug output from stdout to stderr before rig_init
  if (hamlib_debug) {
    rig_set_debug_level(RIG_DEBUG_TRACE);
    rig_set_debug_file(stderr);
  }

  std::cerr << "[RIG] Initializing Hamlib model ID " << model_ << "...\n";
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
  shutdown();
  if (rig_) {
    set_ptt(false);
    rig_close(rig_);
    rig_cleanup(rig_);
  }
}

bool RadioController::initialize() {
  // Base initialization - just return true since constructor opens rig
  // THD75 should override to add THD75-specific initialization
  return rig_ != nullptr;
}

void RadioController::shutdown() {
  // Base shutdown - just close the rig
  // THD75 should override to add THD75-specific cleanup
  if (rig_) {
    std::cerr << "[RIG] Base shutdown - closing radio connection.\n";
  }
}

// Virtual method implementations that THD75 can override as needed

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

  std::cerr << "[RIG] Setting TX power to " << lvl << "...\n";
  return true;
}

bool RadioController::get_power_level(std::string &level) {
  // Base implementation returns false - THD75 should override
  return false;
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
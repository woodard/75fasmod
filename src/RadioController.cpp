#include "RadioController.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
constexpr double FREQUENCY_MHZ_TO_HZ = 1000000.0;
constexpr size_t BUFFER_SIZE = 64;
constexpr unsigned int HEX_BASE = 16;
} // namespace

auto RadioController::read_sysfs_attr(const fs::path &filepath) -> std::string {
  std::ifstream file(filepath);
  std::string value;
  if (file >> value) {
    return value;
  }
  return "";
}

RadioController::RadioController(rig_model_t model, std::string port,
                                 bool hamlib_debug)
    : model_(model), port_(std::move(port)), rig_(nullptr),
      current_mode_(Mode::FM), orig_mode_(RIG_MODE_NONE),
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
  if (rig_ == nullptr) {
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
  if (rig_ != nullptr) {
    set_ptt(false);
    rig_close(rig_);
    rig_cleanup(rig_);
  }
}

auto RadioController::initialize() -> bool {
  // Base initialization - just return true since constructor opens rig
  // THD75 should override to add THD75-specific initialization
  return rig_ != nullptr;
}

void RadioController::shutdown() {
  // Base shutdown - close the rig connection
  // THD75 should override to add THD75-specific cleanup BEFORE calling this
  if (rig_ != nullptr) {
    set_ptt(false);
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;
  }
}

auto RadioController::set_frequency(double freq_mhz) -> bool {
  auto freq_hz = static_cast<freq_t>(freq_mhz * FREQUENCY_MHZ_TO_HZ);
  return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

auto RadioController::get_frequency(double &freq_mhz) -> bool {
  freq_t freq_hz;
  if (rig_get_freq(rig_, RIG_VFO_CURR, &freq_hz) != RIG_OK) {
    return false;
  }
  freq_mhz = static_cast<double>(freq_hz) / FREQUENCY_MHZ_TO_HZ;
  return true;
}

auto RadioController::get_mode(Mode &mode) -> bool {
  rmode_t rig_mode = 0;
  int result = rig_get_mode(rig_, RIG_VFO_CURR, &rig_mode, nullptr);
  if (result == RIG_OK) {
    mode = static_cast<Mode>(rig_mode);
    std::cerr << "[RIG] get_mode succeeded, mode value: "
              << static_cast<int>(mode) << '\n';
    return true;
  }

  // Fallback: return the current mode we've tracked
  std::cerr << "[RIG] get_mode failed (fallback), returning tracked mode: "
            << static_cast<int>(current_mode_) << '\n';
  mode = current_mode_;
  return true;
}

auto RadioController::set_mode(Mode mode) -> bool {
  bool result =
      rig_set_mode(rig_, RIG_VFO_CURR, static_cast<rmode_t>(mode), 0) == RIG_OK;
  if (result) {
    current_mode_ = mode; // Save the mode we just set
  }
  return result;
}

auto RadioController::set_ptt(bool transmit) -> bool {
  const char *cmd = transmit ? "TX\r" : "RX\r";
  std::array<char, BUFFER_SIZE> buf{};
  unsigned char term = '\r';

  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           static_cast<int>(strlen(cmd)),
                           reinterpret_cast<unsigned char *>(buf.data()),
                           static_cast<int>(sizeof(buf) - 1), &term);

  return (bytes >= 0 || bytes == -RIG_ETIMEOUT || bytes == RIG_ETIMEOUT);
}

auto RadioController::get_dcd(bool &is_squelch_open) -> bool {
  dcd_t dcd_status;
  if (rig_get_dcd(rig_, RIG_VFO_CURR, &dcd_status) == RIG_OK) {
    is_squelch_open = (dcd_status == RIG_DCD_ON);
    return true;
  }
  return false;
}

auto RadioController::set_power_level(const std::string &level) -> bool {
  std::string lvl = level;
  for (auto &chr : lvl) {
    chr = static_cast<char>(std::toupper(chr));
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

  // Suppress unused variable warning - this is a placeholder for derived
  // classes
  (void)val;

  std::cerr << "[RIG] Setting TX power to " << lvl << "...\n";
  return true;
}

auto RadioController::get_power_level(std::string & /*level*/) -> bool {
  // Base implementation returns false - THD75 should override
  return false;
}

auto RadioController::find_tty_sysfs(unsigned int target_vid,
                                     unsigned int target_pid)
    -> std::vector<std::string> {
  std::vector<std::string> found_ports;
  fs::path sys_tty = "/sys/class/tty";

  if (!fs::exists(sys_tty)) {
    return found_ports;
  }

  for (const auto &entry : fs::directory_iterator(sys_tty)) {
    fs::path dev_path = entry.path() / "device";
    if (!fs::exists(dev_path)) {
      continue;
    }

    for (const char *parent_rel : {"..", "../..", "../../.."}) {
      fs::path vid_path = dev_path / parent_rel / "idVendor";
      fs::path pid_path = dev_path / parent_rel / "idProduct";

      if (fs::exists(vid_path) && fs::exists(pid_path)) {
        unsigned int vid = 0;
        unsigned int pid = 0;
        if (auto vid_str = read_sysfs_attr(vid_path); !vid_str.empty()) {
          vid = std::stoul(vid_str, nullptr, HEX_BASE);
        }
        if (auto pid_str = read_sysfs_attr(pid_path); !pid_str.empty()) {
          pid = std::stoul(pid_str, nullptr, HEX_BASE);
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

auto RadioController::find_alsa_device(const std::string &serial_port)
    -> std::string {
  fs::path tty_name = fs::path(serial_port).filename();
  fs::path tty_dev_path = "/sys/class/tty" / tty_name / "device";

  if (!fs::exists(tty_dev_path)) {
    return "";
  }

  fs::path usb_dev_path;
  try {
    usb_dev_path = fs::canonical(tty_dev_path).parent_path();
  } catch (...) {
    return "";
  }

  fs::path sound_class_path = "/sys/class/sound";
  if (!fs::exists(sound_class_path)) {
    return "";
  }

  for (const auto &entry : fs::directory_iterator(sound_class_path)) {
    std::string card_name = entry.path().filename().string();

    if (card_name.starts_with("card")) {
      fs::path card_dev_path = entry.path() / "device";
      if (!fs::exists(card_dev_path)) {
        continue;
      }

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

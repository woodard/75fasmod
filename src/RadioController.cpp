/**
 * @file RadioController.cpp
 * @brief Base radio controller for Hamlib interface
 *
 * Provides a high-level C++ abstraction over the Hamlib C API for controlling
 * radio transceivers. Handles initialization, state backup/restoration, and
 * safe serial port management.
 */

#include "RadioController.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace {
constexpr double FREQUENCY_MHZ_TO_HZ = 1000000.0;
constexpr size_t BUFFER_SIZE = 64;
constexpr unsigned int HEX_BASE = 16;
} // namespace

/**
 * @brief Helper function to read sysfs attributes
 *
 * @param filepath Path to the sysfs file
 * @return String value read from file, or empty string on error
 */
auto RadioController::read_sysfs_attr(const fs::path &filepath) -> std::string {
  std::ifstream file(filepath);
  std::string value;
  if (file >> value) {
    return value;
  }
  return "";
}

/**
 * @brief Flushes stale input bytes from the radio serial port
 *
 * Uses the modern Hamlib rig_data_pointer API to extract the serial port
 * reference and calls rig_flush to clear operating system serial buffers.
 */
void RadioController::flush_serial() {
  if (rig_ != nullptr) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    rig_flush(static_cast<hamlib_port_t *>(
        rig_data_pointer(rig_, RIG_PTRX_RIGPORT)));
  }
}

/**
 * @brief Construct a new Radio Controller object
 *
 * Initializes the Hamlib rig instance, forcibly drains lingering OS serial 
 * buffers to prevent protocol desynchronization, and safely kills Hamlib's 
 * internal background cache polling thread to avoid collisions.
 *
 * @param model Hamlib rig model number
 * @param port Serial port device path
 * @param hamlib_debug Enable Hamlib debug output
 */
RadioController::RadioController(rig_model_t model, std::string port,
                                 bool hamlib_debug)
    : model_(model), port_(std::move(port)), rig_(nullptr),
      current_mode_(Mode::FM), orig_mode_(RIG_MODE_NONE),
      orig_mode_saved_(false), orig_frequency_(0), orig_frequency_saved_(false),
      orig_width_(0), orig_power_(PowerLevel::UNKNOWN),
      orig_power_saved_(false) {
  
  if (hamlib_debug) {
    rig_set_debug_level(RIG_DEBUG_TRACE);
    rig_set_debug_file(stderr);
  }

  std::cerr << "[RIG] Initializing Hamlib model ID " << model_ << "...\n";
  rig_ = rig_init(model_);
  if (rig_ == nullptr) {
    std::cerr << "[RIG] Error: rig_init() failed for model ID " << model_ << "\n";
    return;
  }

  rig_set_conf(rig_, rig_token_lookup(rig_, "rig_pathname"), port_.c_str());

  // Single open call handled by Hamlib (thd75_open will handle the 200ms DTR flush)
  int status = rig_open(rig_);
  if (status != RIG_OK) {
    std::cerr << "[RIG] Error: rig_open() failed on " << port_
              << " | Code: " << status << " (" << rigerror(status) << ")\n";
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;
    return;
  }

  // Disable Hamlib background cache polling thread globally
  rig_set_cache_timeout_ms(rig_, static_cast<hamlib_cache_t>(0), 0);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  flush_serial();
}

/**
 * @brief Destroy the Radio Controller object
 */
RadioController::~RadioController() {
  if (rig_ != nullptr) {
    std::cerr << "[RIG] Closing connection..." << std::endl;
    set_ptt(false);
    rig_close(rig_);
    rig_cleanup(rig_);
    rig_ = nullptr;
  }
}

/**
 * @brief Initialize the radio controller and backup state
 *
 * Backs up the original frequency and mode so they can be seamlessly
 * restored on shutdown.
 *
 * @return true if initialization successful, false otherwise
 */
auto RadioController::initialize() -> bool {
  if (rig_ == nullptr) {
    return false;
  }

  std::cerr << "[RIG] Saving original radio state..." << std::endl;

  // Save original frequency using virtual override (invokes THD75 CAT fallback if applicable)
  double freq_mhz = 0.0;
  if (this->get_frequency(freq_mhz)) {
    orig_frequency_ = static_cast<freq_t>(freq_mhz * FREQUENCY_MHZ_TO_HZ);
    orig_frequency_saved_ = true;
    std::cerr << "[RIG] Saved frequency: " << freq_mhz << " MHz" << std::endl;
  }

  // Save original mode and bandwidth using virtual override
  Mode mode = Mode::FM;
  if (this->get_mode(mode)) {
    orig_mode_ = static_cast<rmode_t>(mode);
    orig_mode_saved_ = true;
    std::cerr << "[RIG] Saved mode: " << static_cast<int>(mode) << std::endl;
  }

  return true;
}

/**
 * @brief Shutdown the radio controller and restore backup state
 */
void RadioController::shutdown() {
  if (rig_ == nullptr) {
    return;
  }

  std::cerr << "[RIG] Restoring radio state..." << std::endl;

  // Restore original frequency using virtual override
  if (orig_frequency_saved_) {
    double freq_mhz = static_cast<double>(orig_frequency_) / FREQUENCY_MHZ_TO_HZ;
    this->set_frequency(freq_mhz);
  }

  // Restore original mode using virtual override
  if (orig_mode_saved_) {
    this->set_mode(static_cast<Mode>(orig_mode_));
  }

  set_ptt(false);
  rig_close(rig_);
  rig_cleanup(rig_);
  rig_ = nullptr;
}

/**
 * @brief Set the radio frequency via Hamlib
 *
 * @param freq_mhz Frequency in megahertz
 * @return true if set successfully, false otherwise
 */
auto RadioController::set_frequency(double freq_mhz) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  auto freq_hz = static_cast<freq_t>(freq_mhz * FREQUENCY_MHZ_TO_HZ);
  return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

/**
 * @brief Get the current VFO frequency via Hamlib
 *
 * @param freq_mhz Reference to store the frequency in megahertz
 * @return true if query successful, false on error
 */
auto RadioController::get_frequency(double &freq_mhz) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  freq_t freq_hz;
  if (rig_get_freq(rig_, RIG_VFO_CURR, &freq_hz) != RIG_OK) {
    return false;
  }
  freq_mhz = static_cast<double>(freq_hz) / FREQUENCY_MHZ_TO_HZ;
  return true;
}

/**
 * @brief Get the current radio mode via Hamlib
 *
 * @param mode Reference to store the current mode
 * @return true if query successful, false on error
 */
auto RadioController::get_mode(Mode &mode) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  rmode_t rig_mode = 0;
  int result = rig_get_mode(rig_, RIG_VFO_CURR, &rig_mode, nullptr);
  if (result == RIG_OK) {
    mode = static_cast<Mode>(rig_mode);
    return true;
  }
  mode = current_mode_;
  return true;
}

/**
 * @brief Set the radio mode via Hamlib
 *
 * @param mode The mode to set
 * @return true if set successfully, false otherwise
 */
auto RadioController::set_mode(Mode mode) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  bool result =
      rig_set_mode(rig_, RIG_VFO_CURR, static_cast<rmode_t>(mode), 0) == RIG_OK;
  if (result) {
    current_mode_ = mode;
  }
  return result;
}

/**
 * @brief Set PTT (Push-to-Talk) state via Hamlib raw write
 *
 * @param transmit true for transmit, false for receive
 * @return true if command successful, false otherwise
 */
auto RadioController::set_ptt(bool transmit) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  const char *cmd = transmit ? "TX\r" : "RX\r";
  std::array<char, BUFFER_SIZE> buf{};
  unsigned char term = '\r';

  int bytes = rig_send_raw(rig_, reinterpret_cast<const unsigned char *>(cmd),
                           static_cast<int>(strlen(cmd)),
                           reinterpret_cast<unsigned char *>(buf.data()),
                           static_cast<int>(sizeof(buf) - 1), &term);

  return (bytes >= 0 || bytes == -RIG_ETIMEOUT || bytes == RIG_ETIMEOUT);
}

/**
 * @brief Get the DCD (Digital Carrier Detect) status
 *
 * @param is_squelch_open Reference to store the DCD status
 * @return true if query successful, false on error
 */
auto RadioController::get_dcd(bool &is_squelch_open) -> bool {
  if (rig_ == nullptr) {
    return false;
  }
  dcd_t dcd_status;
  if (rig_get_dcd(rig_, RIG_VFO_CURR, &dcd_status) == RIG_OK) {
    is_squelch_open = (dcd_status == RIG_DCD_ON);
    return true;
  }
  return false;
}

/**
 * @brief Set the TX power level (Base class placeholder)
 */
auto RadioController::set_power_level(const std::string &) -> bool { return false; }

/**
 * @brief Get the current TX power level (Base class placeholder)
 */
auto RadioController::get_power_level(std::string &) -> bool { return false; }

/**
 * @brief Find tty sysfs devices for a specific USB device PID/VID
 *
 * Searches for serial devices matching the given USB Vendor ID and Product ID.
 *
 * @param target_vid USB Vendor ID
 * @param target_pid USB Product ID
 * @return Vector of device paths
 */
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

/**
 * @brief Find ALSA device mapped to the serial port's parent USB hub
 *
 * Searches for an ALSA sound device associated with the radio's USB serial port.
 *
 * @param serial_port Serial port path
 * @return ALSA device name
 */
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

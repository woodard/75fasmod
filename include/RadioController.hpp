/**
 * @file RadioController.hpp
 * @brief Radio controller for Kenwood TH-D75 using Hamlib
 *
 * This module provides a high-level interface to control the Kenwood TH-D75
 * radio transceiver using the Hamlib library. It handles frequency setting
 * and querying, power level management, PTT control, and USB interface
 * configuration for digital modem operation.
 *
 * @author 75fasmod developers
 * @version 0.1.0
 * @date 2024
 */

#ifndef RADIOCONTROLLER_HPP
#define RADIOCONTROLLER_HPP

#include <filesystem>
#include <fstream>
#include <functional>
#include <hamlib/rig.h>
#include <matvec.hpp>
#include <matvec/tcd.hpp>
#include <memory>
#include <pillow.hpp>
#include <pillow/pillow.hpp>
#include <set>
#include <string>
#include <sys/socket.h>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Radio controller class for Kenwood TH-D75
 *
 * This class provides methods to control a Kenwood TH-D75 radio transceiver
 * through Hamlib. It handles initialization, frequency control, power level
 * management, PTT control, and USB interface configuration for digital
 * modem operation.
 *
 * @note Requires Hamlib library with Kenwood TH-D75 backend support
 * @note Currently supports Kenwood TH-D75 (rig_model_t:
 * RIG_MODEL_KENWOOD_TH_D75)
 */
class RadioController {
public:
  /**
   * @brief Power level enumeration for TX power control
   *
   * Maps to Kenwood CAT power level values for the TH-D75 radio.
   */
  enum class PowerLevel {
    HIGH = 0,      ///< High power (maximum TX power)
    MEDIUM = 1,    ///< Medium power
    LOW = 2,       ///< Low power
    EXTRA_LOW = 3, ///< Extra low power
    UNKNOWN = -1   ///< Unknown or uninitialized power level
  };

  /**
   * @brief USB Out Select enumeration for Menu Item 102
   *
   * Controls which signal path is routed to the USB/serial data port.
   * This is essential for digital modem operation.
   */
  enum class UsbOutSelect {
    AF = 0,     ///< Audio Frequency output (demodulated audio)
    IF = 1,     ///< Intermediate Frequency output (raw signal)
    Detect = 2, ///< Detection signal
    unknown     ///< Unknown or undefined value
  };

  /**
   * @brief Radio mode enumeration
   *
   * Radio modes mapped to Hamlib RIG_MODE_* constants.
   * Note: These are Hamlib mode flags (bit flags), not simple enums.
   */
  enum class Mode : rmode_t {
    FM = RIG_MODE_FM,   ///< FM mode
    WFM = RIG_MODE_WFM, ///< Wide FM mode
    AM = RIG_MODE_AM,   ///< AM mode
    USB = RIG_MODE_USB, ///< Upper Sideband
    LSB = RIG_MODE_LSB, ///< Lower Sideband
    CW = RIG_MODE_CW,   ///< Morse code
    CWR = RIG_MODE_CWR, ///< CW reverse
    DD = RIG_MODE_DD    ///< Digital voice
  };

  /**
   * @brief Construct a new Radio Controller object
   *
   * @param model Hamlib rig model number (e.g., RIG_MODEL_KENWOOD_TH_D75)
   * @param port Serial port device path (e.g., "/dev/ttyUSB0")
   * @param hamlib_debug Enable Hamlib debug output (default: false)
   */
  RadioController(rig_model_t model, std::string port,
                  bool hamlib_debug = false);

  /**
   * @brief Shutdown the radio controller
   *
   * Closes the radio connection and performs cleanup.
   * Derived classes should override to add specific cleanup logic.
   */
  virtual void shutdown();

  /**
   * @brief Destroy the Radio Controller object
   *
   * Closes the Hamlib rig connection if open.
   */
  virtual ~RadioController();

  /**
   * @brief Initialize the radio controller
   *
   * Base initialization. Derived classes should override to add
   * THD75-specific initialization logic.
   *
   * @return true if initialization successful, false otherwise
   */
  virtual bool initialize();

  /**
   * @brief Set the radio frequency
   *
   * Sets the VFO frequency in megahertz.
   *
   * @param freq_mhz Frequency in megahertz
   * @return true if set successfully, false otherwise
   */
  virtual bool set_frequency(double freq_mhz);

  /**
   * @brief Get the current VFO frequency
   *
   * Queries the radio for the current VFO frequency in megahertz.
   *
   * @param freq_mhz Reference to store the frequency in megahertz
   * @return true if query successful, false on error
   */
  virtual bool get_frequency(double &freq_mhz);

  /**
   * @brief Get the current radio mode
   *
   * Queries the radio for the current operating mode for the current VFO.
   *
   * @param mode Reference to store the current mode
   * @return true if query successful, false on error
   */
  virtual bool get_mode(Mode &mode);

  /**
   * @brief Set the radio mode
   *
   * Sets the operating mode for the current VFO.
   *
   * @param mode The mode to set
   * @return true if set successfully, false otherwise
   */
  virtual bool set_mode(Mode mode);

  /**
   * @brief Set the TX power level
   *
   * Sets the transmission power level. Valid levels: "EL", "L", "M", "H".
   *
   * @param level Power level string ("EL", "L", "M", or "H")
   * @return true if set successfully, false otherwise
   */
  virtual bool set_power_level(const std::string &level);

  /**
   * @brief Get the current TX power level
   *
   * Queries the radio for the current TX power level.
   *
   * @param level Reference to store the power level string ("EL", "L", "M",
   * "H")
   * @return true if query successful, false on error
   */
  virtual bool get_power_level(std::string &level);

  /**
   * @brief Set PTT (Push-to-Talk) state
   *
   * Controls the transmit/receive state of the radio.
   *
   * @param transmit true for transmit, false for receive
   * @return true if command successful, false otherwise
   */
  virtual bool set_ptt(bool transmit);

  /**
   * @brief Get the DCD (Digital Carrier Detect) status
   *
   * Queries whether carrier is detected on the receive path.
   *
   * @param is_squelch_open Reference to store the DCD status
   * @return true if query successful, false on error
   */
  virtual bool get_dcd(bool &is_squelch_open);

  /**
   * @brief Find available serial tty devices in sysfs
   *
   * Searches /sys/class/tty for serial devices matching the TH-D75 USB PID/VID.
   *
   * @return Vector of device paths matching the TH-D75
   */
  static std::vector<std::string> find_tty_sysfs() {
    return find_tty_sysfs(0x2166, 0x9023);
  }

  std::string find_alsa_device() { return find_alsa_device(port_); }

protected:
  /**
   * @brief Find tty sysfs devices for a specific USB device
   *
   * Searches for serial devices matching the given USB Vendor ID and Product
   * ID.
   *
   * @param target_vid USB Vendor ID
   * @param target_pid USB Product ID
   * @return Vector of device paths
   */
  static std::vector<std::string> find_tty_sysfs(unsigned int target_vid,
                                                 unsigned int target_pid);

  /**
   * @brief Find ALSA device for the radio
   *
   * Searches for an ALSA sound device associated with the radio's USB serial
   * port.
   *
   * @param port Serial port path
   * @return ALSA device name
   */
  static std::string find_alsa_device(const std::string &port);

  /**
   * @brief Helper function to read sysfs attributes
   *
   * Reads a string value from a sysfs file.
   *
   * @param filepath Path to the sysfs file
   * @return String value read from file, or empty string on error
   */
  static std::string read_sysfs_attr(const fs::path &filepath);

  rig_model_t model_; ///< Hamlib rig model
  std::string port_;  ///< Serial port path

protected:
  RIG *rig_;          ///< Hamlib rig handle
  Mode current_mode_; ///< Current radio mode (tracked for compatibility)

  // Backup variables for restoring radio state
  rmode_t orig_mode_;    ///< Saved original radio mode
  bool orig_mode_saved_;    ///< Flag indicating if mode was saved
  freq_t orig_frequency_;   ///< Saved original frequency (Hz)
  bool orig_frequency_saved_; ///< Flag indicating if frequency was saved

  pbwidth_t orig_width_;  ///< Saved original bandwidth
  PowerLevel orig_power_;     ///< Saved original power level
  bool orig_power_saved_;     ///< Flag indicating if power level was saved
};

#endif // RADIOCONTROLLER_HPP

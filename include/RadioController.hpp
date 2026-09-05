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
   * @brief Default Hamlib model for Kenwood TH-D75
   *
   * This is the Hamlib model ID for the Kenwood TH-D75 transceiver.
   */
  static constexpr rig_model_t DEFAULT_MODEL = 2042;

  /**
   * @brief Construct a new Radio Controller object
   *
   * @param model Hamlib rig model number (e.g., RIG_MODEL_KENWOOD_TH_D75)
   * @param port Serial port device path (e.g., "/dev/ttyUSB0")
   */
  RadioController(rig_model_t model, const std::string &port);

  /**
   * @brief Destroy the Radio Controller object
   *
   * Closes the Hamlib rig connection if open.
   */
  ~RadioController();

  /**
   * @brief Initialize the radio controller
   *
   * Opens the connection to the radio, saves current settings, and configures
   * the radio for high-speed modem operation by setting USB Out Select to IF.
   *
   * @param hamlib_debug Enable Hamlib debug output (default: false)
   * @return true if initialization successful, false otherwise
   *
   * @note This method modifies radio settings. Use save/restore methods if
   *       you need to restore settings later.
   */
  bool initialize(bool hamlib_debug = false);

  /**
   * @brief Set the radio frequency
   *
   * Sets the VFO frequency in megahertz.
   *
   * @param freq_mhz Frequency in megahertz
   * @return true if set successfully, false otherwise
   */
  bool set_frequency(double freq_mhz);

  /**
   * @brief Get the current VFO frequency
   *
   * Queries the radio for the current VFO frequency in megahertz.
   *
   * @param freq_mhz Reference to store the frequency in megahertz
   * @return true if query successful, false on error
   */
  bool get_frequency(double &freq_mhz);

  /**
   * @brief Set the TX power level
   *
   * Sets the transmission power level. Valid levels: "EL", "L", "M", "H".
   *
   * @param level Power level string ("EL", "L", "M", or "H")
   * @return true if set successfully, false otherwise
   */
  bool set_power_level(const std::string &level);

  /**
   * @brief Get the current TX power level
   *
   * Queries the radio for the current TX power level.
   *
   * @param level Reference to store the power level string ("EL", "L", "M", or
   * "H")
   * @return true if query successful, false on error
   */
  bool get_power_level(std::string &level);

  /**
   * @brief Set PTT (Push-to-Talk) state
   *
   * Controls the transmit/receive state of the radio.
   *
   * @param transmit true for transmit, false for receive
   * @return true if command successful, false otherwise
   */
  bool set_ptt(bool transmit);

  /**
   * @brief Get the DCD (Digital Carrier Detect) status
   *
   * Queries whether carrier is detected on the receive path.
   *
   * @param is_squelch_open Reference to store the DCD status
   * @return true if query successful, false on error
   */
  bool get_dcd(bool &is_squelch_open);

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

private:
  /**
   * @brief Get the Kenwood TNC mode
   *
   * Queries the TNC (Telegraph Noiseless Code) operating mode.
   *
   * @return TNC mode value, or -1 on error
   */
  int kenwood_tnc_get();

  /**
   * @brief Set the Kenwood TNC mode
   *
   * Sets the TNC operating mode.
   *
   * @param mode TNC mode value
   * @return true if successful, false otherwise
   */
  bool kenwood_tnc_set(int mode);

  /**
   * @brief Get the USB Out Select setting
   *
   * Queries Menu Item 102 to determine the current USB data output path.
   *
   * @return UsbOutSelect enum value
   */
  UsbOutSelect kenwood_usb_out_select_get();

  /**
   * @brief Set the USB Out Select setting
   *
   * Sets Menu Item 102 to control USB data output path.
   * Set to IF for digital modem operation.
   *
   * @param value UsbOutSelect enum value
   * @return true if successful, false otherwise
   */
  bool kenwood_usb_out_select_set(UsbOutSelect value);

  /**
   * @brief Get the current power level
   *
   * Internal method to query the radio's power level.
   *
   * @return PowerLevel enum value
   */
  PowerLevel kenwood_power_get();

  /**
   * @brief Set the power level
   *
   * Internal method to set the radio's power level.
   *
   * @param val PowerLevel enum value
   * @return true if successful, false otherwise
   */
  bool kenwood_power_set(PowerLevel val);

  /**
   * @brief Get a Kenwood menu item
   *
   * Queries a Kenwood CAT menu item by number.
   *
   * @param menu_num Menu item number
   * @return Menu value, or -1 on error
   */
  int kenwood_menu_get(int menu_num);

  /**
   * @brief Set a Kenwood menu item
   *
   * Sets a Kenwood CAT menu item by number and value.
   *
   * @param menu_num Menu item number
   * @param value Menu value to set
   * @return true if successful, false otherwise
   */
  bool kenwood_menu_set(int menu_num, int value);

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

private:
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
  RIG *rig_;          ///< Hamlib rig handle

  rmode_t orig_mode_;    ///< Saved original radio mode
  bool orig_mode_saved_; ///< Flag indicating if mode was saved

  vfo_t orig_vfo_;             ///< Saved original VFO state
  pbwidth_t orig_width_;       ///< Saved original bandwidth
  PowerLevel orig_power_;      ///< Saved original power level
  UsbOutSelect orig_menu_102_; ///< Saved original USB Out Select setting
  int orig_tnc_state_;         ///< Saved original TNC state
};

#endif // RADIOCONTROLLER_HPP

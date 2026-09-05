/**
 * @file THD75.hpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 *
 * This class implements the TH-D75 specific functionality for the
 * RadioController base class. It provides implementations using
 * Hamlib's Kenwood TH-D75 backend where needed.
 */

#ifndef THD75_HPP
#define THD75_HPP

#include "RadioController.hpp"

class THD75 : public RadioController {
public:
  /**
   * @brief Default Hamlib model for Kenwood TH-D75
   *
   * This is the Hamlib model ID for the Kenwood TH-D75 transceiver.
   */
  static constexpr rig_model_t DEFAULT_MODEL = 2042;

  /**
   * @brief Construct a new TH D75 object
   *
   * @param port Serial port device path (e.g., "/dev/ttyUSB0")
   * @param model Hamlib rig model number (default: RIG_MODEL_KENWOOD_TH_D75)
   * @param hamlib_debug Enable Hamlib debug output (default: false)
   */
  THD75(const std::string &port, rig_model_t model = DEFAULT_MODEL,
        bool hamlib_debug = false);

  /**
   * @brief Initialize the THD75 radio controller
   *
   * Saves current radio settings and configures the radio for
   * high-speed modem operation by setting USB Out Select to IF.
   *
   * @return true if initialization successful, false otherwise
   */
  virtual bool initialize();

  /**
   * @brief Shutdown the THD75 radio controller
   *
   * Restores the radio to its original state after modifications.
   */
  virtual void shutdown();

  // THD75-specific implementations
  virtual bool set_ptt(bool transmit);
  virtual bool set_power_level(const std::string &level);
  virtual bool get_power_level(std::string &level);

private:
  // Kenwood-specific helper methods

  /**
   * @brief Get the Kenwood TNC mode
   */
  int kenwood_tnc_get();

  /**
   * @brief Set the Kenwood TNC mode
   */
  bool kenwood_tnc_set(int mode);

  /**
   * @brief Get the USB Out Select setting
   */
  UsbOutSelect kenwood_usb_out_select_get();

  /**
   * @brief Set the USB Out Select setting
   */
  bool kenwood_usb_out_select_set(UsbOutSelect value);

  /**
   * @brief Get the current power level
   */
  PowerLevel kenwood_power_get();

  /**
   * @brief Set the power level
   */
  bool kenwood_power_set(PowerLevel val);

  /**
   * @brief Get a Kenwood menu item
   */
  int kenwood_menu_get(int menu_num);

  /**
   * @brief Set a Kenwood menu item
   */
  bool kenwood_menu_set(int menu_num, int value);

  // THD75-specific state backup variables
  UsbOutSelect orig_menu_102_; ///< Saved original USB Out Select setting
  int orig_tnc_state_;         ///< Saved original TNC state
};

#endif // THD75_HPP

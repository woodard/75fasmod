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
   * @brief VFO selection enum for TH-D75
   *
   * Values match RIG_VFO_A and RIG_VFO_B from hamlib.
   * RIG_VFO_A = 1 (1<<0), RIG_VFO_B = 2 (1<<1)
   */
  enum class VFO : int8_t { A = RIG_VFO_A, B = RIG_VFO_B };

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
  bool initialize() override;

  /**
   * @brief Shutdown the THD75 radio controller
   *
   * Restores the radio to its original state after modifications.
   */
  void shutdown() override;

  // THD75-specific implementations
  bool set_ptt(bool transmit) override;
  bool set_power_level(const std::string &level) override;
  bool get_power_level(std::string &level) override;

  /**
   * @brief Get the current VFO that will transmit when PTT is set
   *
   * @return VFO::A if VFO A will transmit, VFO::B if VFO B will transmit
   */
  auto get_current_vfo() -> VFO;

  /**
   * @brief Set the VFO that will transmit when PTT is set
   *
   * @param vfo The VFO to set (VFO::A or VFO::B)
   * @return true if successful, false otherwise
   */
  auto set_current_vfo(VFO vfo) -> bool;

  /**
   * @brief Check if the radio is in Single Band mode
   *
   * Uses RIG_FUNC_DUAL_WATCH to determine dual watch status.
   *
   * @return true if in Single Band mode, false if in Dual Band mode
   */
  auto get_single() -> bool;

  /**
   * @brief Set the radio to Single Band mode
   *
   * First sets the current VFO, then disables DUAL_WATCH function.
   *
   * @param vfo The VFO to use (VFO::A or VFO::B)
   * @return true if successful, false otherwise
   */
  auto set_single(VFO vfo) -> bool;

  /**
   * @brief Check if the radio is in Dual Band mode
   *
   * @return true if in Dual Band mode, false if in Single Band mode
   */
  auto get_dual() -> bool;

  /**
   * @brief Set the radio to Dual Band mode
   *
   * Enables the DUAL_WATCH function for dual band reception.
   *
   * @return true if successful, false otherwise
   */
  auto set_dual() -> bool;

  /**
   * @brief Set single or dual mode based on current state
   *
   * If currently in single mode, switches to dual. If in dual mode,
   * switches to single (sets VFO to specified value first).
   *
   * @param vfo The VFO to set if switching to single mode
   * @return true if successful, false otherwise
   */
  auto flip_single_dual(VFO vfo) -> bool;

  /**
   * @brief Set the mode on the other VFO (not currently transmitting)
   *
   * Gets the current VFO that will transmit, switches to the opposite VFO,
   * sets the mode there, then restores the original VFO.
   *
   * @param mode The mode to set on the other VFO
   * @return true if successful, false otherwise
   */
  auto set_other_mode(Mode mode) -> bool;

  /**
   * @brief Get the mode from the other VFO (not currently transmitting)
   *
   * Gets the current VFO that will transmit, switches to the opposite VFO,
   * queries the mode there, then restores the original VFO.
   *
   * @param mode Reference to store the mode from the other VFO
   * @return true if successful, false otherwise
   */
  auto get_other_mode() -> Mode;

  // TNC control methods (public for external access)
  /**
   * @brief Get the Kenwood TNC mode
   * @return TNC mode status
   */
  auto get_tnc() -> int;

  /**
   * @brief Set the Kenwood TNC mode
   * @param mode The TNC mode to set
   * @return true if successful, false otherwise
   */
  auto set_tnc(int mode) -> bool;

private:
  // Kenwood-specific helper methods
  /**
   * @brief Get the USB Out Select setting
   */
  auto kenwood_usb_out_select_get() -> UsbOutSelect;

  /**
   * @brief Set the USB Out Select setting
   */
  auto kenwood_usb_out_select_set(UsbOutSelect value) -> bool;

  /**
   * @brief Get the current power level
   */
  auto kenwood_power_get() -> PowerLevel;

  /**
   * @brief Set the power level
   */
  auto kenwood_power_set(PowerLevel val) -> bool;

  /**
   * @brief Get a Kenwood menu item
   */
  auto kenwood_menu_get(int menu_num) -> int;

  /**
   * @brief Set a Kenwood menu item
   */
  auto kenwood_menu_set(int menu_num, int value) -> bool;

  // THD75-specific state backup variables
  UsbOutSelect orig_menu_102_; ///< Saved original USB Out Select setting
  vfo_t orig_vfo_;                 ///< Saved original VFO state
};

#endif // THD75_HPP

/**
 * @file THD75.hpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 *
 * This class implements the TH-D75 specific functionality for the
 * RadioController base class. It provides implementations for all
 * virtual methods using Hamlib's Kenwood TH-D75 backend.
 */

#ifndef THD75_HPP
#define THD75_HPP

#include "RadioController.hpp"

class THD75 : public RadioController {
public:
  /**
   * @brief Construct a new TH D75 object
   *
   * @param model Hamlib rig model number (RIG_MODEL_KENWOOD_TH_D75)
   * @param port Serial port device path (e.g., "/dev/ttyUSB0")
   * @param hamlib_debug Enable Hamlib debug output (default: false)
   */
  THD75(rig_model_t model, const std::string &port,
        bool hamlib_debug = false);

  // Implement virtual methods
  bool set_frequency(double freq_mhz) override;
  bool get_frequency(double &freq_mhz) override;
  bool set_power_level(const std::string &level) override;
  bool get_power_level(std::string &level) override;
  bool set_ptt(bool transmit) override;
  bool get_dcd(bool &is_squelch_open) override;

protected:
  // Kenwood-specific implementations
  int kenwood_tnc_get();
  bool kenwood_tnc_set(int mode);
  UsbOutSelect kenwood_usb_out_select_get();
  bool kenwood_usb_out_select_set(UsbOutSelect value);
  PowerLevel kenwood_power_get();
  bool kenwood_power_set(PowerLevel val);
  int kenwood_menu_get(int menu_num);
  bool kenwood_menu_set(int menu_num, int value);
};

#endif // THD75_HPP

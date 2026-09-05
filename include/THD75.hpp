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
   * @brief Construct a new TH D75 object
   *
   * @param model Hamlib rig model number (RIG_MODEL_KENWOOD_TH_D75)
   * @param port Serial port device path (e.g., "/dev/ttyUSB0")
   * @param hamlib_debug Enable Hamlib debug output (default: false)
   */
  THD75(rig_model_t model, const std::string &port, bool hamlib_debug = false);

  // THD75-specific implementations
  // Note: set_frequency, get_frequency, set_power_level, get_power_level,
  // and get_dcd are inherited from RadioController as the base class
  // implementations are sufficient for THD75.

  bool set_ptt(bool transmit) override;
};

#endif // THD75_HPP

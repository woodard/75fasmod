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

  // THD75-specific implementations
  // Note: set_frequency, get_frequency, set_power_level, get_power_level,
  // and get_dcd are inherited from RadioController as the base class
  // implementations are sufficient for THD75.

  bool set_ptt(bool transmit) override;
};

#endif // THD75_HPP

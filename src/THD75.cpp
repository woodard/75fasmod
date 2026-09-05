/**
 * @file THD75.cpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 */

#include "THD75.hpp"

// Constructor
THD75::THD75(const std::string &port, rig_model_t model, bool hamlib_debug)
    : RadioController(model, port, hamlib_debug) {}

// THD75-specific implementation using Hamlib's PTT function
bool THD75::set_ptt(bool transmit) {
  return rig_set_ptt(rig_, RIG_VFO_CURR, transmit ? RIG_PTT_ON : RIG_PTT_OFF) ==
         RIG_OK;
}

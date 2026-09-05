/**
 * @file THD75.cpp
 * @brief Kenwood TH-D75 specific radio controller implementation
 */

#include "THD75.hpp"
#include <chrono>
#include <thread>

// Constructor
THD75::THD75(rig_model_t model, const std::string &port, bool hamlib_debug)
    : RadioController(model, port, hamlib_debug) {
}

// Implement virtual methods
bool THD75::set_frequency(double freq_mhz) {
  freq_t freq_hz = static_cast<freq_t>(freq_mhz * 1000000.0);
  return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

bool THD75::get_frequency(double &freq_mhz) {
  freq_t freq_hz;
  if (rig_get_freq(rig_, RIG_VFO_CURR, &freq_hz) != RIG_OK) {
    return false;
  }
  freq_mhz = static_cast<double>(freq_hz) / 1000000.0;
  return true;
}

bool THD75::set_power_level(const std::string &level) {
  return false; // TODO: implement
}

bool THD75::get_power_level(std::string &level) {
  return false; // TODO: implement
}

bool THD75::set_ptt(bool transmit) {
  return rig_set_ptt(rig_, RIG_VFO_CURR, transmit ? RIG_PTT_ON : RIG_PTT_OFF) == RIG_OK;
}

bool THD75::get_dcd(bool &is_squelch_open) {
  dcd_t dcd_status;
  if (rig_get_dcd(rig_, RIG_VFO_CURR, &dcd_status) == RIG_OK) {
    is_squelch_open = (dcd_status == RIG_DCD_ON);
    return true;
  }
  return false;
}

// Kenwood helper functions (protected in header)
int THD75::kenwood_tnc_get() { return 0; }
bool THD75::kenwood_tnc_set(int mode) { return true; }
RadioController::UsbOutSelect THD75::kenwood_usb_out_select_get() { 
  return RadioController::UsbOutSelect::unknown; 
}
bool THD75::kenwood_usb_out_select_set(UsbOutSelect value) { 
  return true; 
}
RadioController::PowerLevel THD75::kenwood_power_get() { 
  return PowerLevel::UNKNOWN; 
}
bool THD75::kenwood_power_set(PowerLevel val) { 
  return true; 
}
int THD75::kenwood_menu_get(int menu_num) { 
  return 0; 
}
bool THD75::kenwood_menu_set(int menu_num, int value) { 
  return true; 
}

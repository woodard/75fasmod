#include "RadioController.hpp"
#include <iostream>
#include <cstring>

RadioController::RadioController(rig_model_t model, const std::string& port)
    : model_(model), port_(port), rig_(nullptr) {}

RadioController::~RadioController() {
    if (rig_) {
        set_ptt(false); // Ensure unkeyed on shutdown
        rig_close(rig_);
        rig_cleanup(rig_);
    }
}

bool RadioController::initialize() {
    rig_ = rig_init(model_);
    if (!rig_) return false;
    strncpy(rig_->state.rigport.pathname, port_.c_str(), FILPATHLEN - 1);
    
    if (rig_open(rig_) != RIG_OK) {
        std::cerr << "Error: Could not open radio on " << port_ << "\n";
        return false;
    }
    return true;
}

bool RadioController::set_frequency(double freq_mhz) {
    freq_t freq_hz = static_cast<freq_t>(freq_mhz * 1000000.0);
    return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

bool RadioController::set_ptt(bool transmit) {
    ptt_t ptt_state = transmit ? RIG_PTT_ON : RIG_PTT_OFF;
    return rig_set_ptt(rig_, RIG_VFO_CURR, ptt_state) == RIG_OK;
}
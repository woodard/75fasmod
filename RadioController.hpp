#pragma once
#include <string>
#include <hamlib/rig.h>

class RadioController {
public:
    RadioController(rig_model_t model, const std::string& port);
    ~RadioController();

    bool initialize();
    bool set_frequency(double freq_mhz);
    bool set_ptt(bool transmit);

private:
    rig_model_t model_;
    std::string port_;
    RIG* rig_;
};
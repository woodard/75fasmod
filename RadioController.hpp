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
    bool get_dcd(bool& is_squelch_open);
  
private:
  // Raw CAT helpers for proprietary Kenwood Menus
  int kenwood_menu_get(int menu_num);
  void kenwood_menu_set(int menu_num, int value);

  rig_model_t model_;
  std::string port_;
  RIG* rig_;

  // --- State Storage for Restoration ---
  rmode_t orig_mode_;
  pbwidth_t orig_width_;
  int orig_menu_102_; // USB Out Select state
};

#pragma once
#include <hamlib/rig.h>
#include <string>
#include <filesystem>
#include <vector>

class RadioController {
public:
  // Define the specific power levels mapped to Kenwood CAT values
  enum class PowerLevel {
    HIGH = 0,
    MEDIUM = 1,
    LOW = 2,
    EXTRA_LOW = 3,
    UNKNOWN = -1
  };

  RadioController(rig_model_t model, const std::string &port);
  ~RadioController();

  bool initialize();
  bool set_frequency(double freq_mhz);
  bool set_power_level(const std::string &level);
  bool set_ptt(bool transmit);
  bool get_dcd(bool &is_squelch_open);

  static std::vector<std::string> find_tty_sysfs();
  static std::vector<std::string> find_tty_sysfs(unsigned int target_vid, unsigned int target_pid);

private:
  int kenwood_menu_get(int menu_num);
  void kenwood_menu_set(int menu_num, int value);

  // Updated Raw CAT helpers using the enum
  PowerLevel kenwood_power_get();
  void kenwood_power_set(PowerLevel val);

  rig_model_t model_;
  std::string port_;
  RIG *rig_;

  rmode_t orig_mode_;
  pbwidth_t orig_width_;
  int orig_menu_102_;
  PowerLevel orig_power_; // Stores the original state using the enum

  std::vector<std::string> find_tty_sysfs(unsigned int target_vid, unsigned int target_pid);
};

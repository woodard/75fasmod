#pragma once
#include <hamlib/rig.h>
#include <string>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class RadioController {
public:
  // Default Hamlib model ID for Kenwood TH-D74 / TH-D75
  static constexpr rig_model_t DEFAULT_MODEL = 2042;

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

  bool initialize(bool hamlib_debug = false);
  bool set_frequency(double freq_mhz);
  bool set_power_level(const std::string &level);
  bool set_ptt(bool transmit);
  bool get_dcd(bool &is_squelch_open);

  static std::vector<std::string> find_tty_sysfs(){
    return find_tty_sysfs(0x2166, 0x9023);
  }
  std::string find_alsa_device(){
    return find_alsa_device(port_);
  }
  
  int kenwood_tnc_get();
  void kenwood_tnc_set(int mode);
  
private:
  PowerLevel kenwood_power_get();
  void kenwood_menu_set(int menu_num, int value);
  int kenwood_menu_get(int menu_num);
  void kenwood_power_set(PowerLevel val);

  rig_model_t model_;
  std::string port_;
  RIG *rig_;

  rmode_t orig_mode_;
  pbwidth_t orig_width_;
  int orig_menu_102_;
  PowerLevel orig_power_; // Stores the original state using the enum

  static std::vector<std::string> find_tty_sysfs(unsigned int target_vid,
						 unsigned int target_pid);
  static std::string find_alsa_device(const std::string& serial_port);
};

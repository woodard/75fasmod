#ifndef THD75_HPP
#define THD75_HPP

#include "RadioController.hpp"

class THD75 : public RadioController {
public:
  static constexpr rig_model_t DEFAULT_MODEL = RIG_MODEL_THD75;

  enum class VFO : int8_t { A = RIG_VFO_A, B = RIG_VFO_B };

  explicit THD75(std::string port, rig_model_t model, bool hamlib_debug);
  ~THD75() override;

  auto initialize() -> bool override;
  void shutdown() override;

  auto set_power_level(const std::string &level) -> bool override;
  auto get_power_level(std::string &level) -> bool override;

  auto set_other_power_level(const std::string &level) -> bool;
  auto get_other_power_level(std::string &level) -> bool;

  auto get_current_vfo() -> VFO;
  auto set_current_vfo(VFO vfo) -> bool;

  auto get_single() -> bool;
  auto set_single(VFO vfo) -> bool;

  auto get_dual() -> bool;
  auto set_dual() -> bool;
  auto flip_single_dual(VFO vfo) -> bool;

  auto set_other_mode(Mode mode) -> bool;
  auto get_other_mode() -> Mode;

  auto set_other_frequency(double freq_mhz) -> bool;
  auto get_other_frequency() -> double;

  auto get_tnc() -> int;
  auto set_tnc(int mode) -> bool;

private:
  auto kenwood_usb_out_select_get() -> UsbOutSelect;
  auto kenwood_usb_out_select_set(UsbOutSelect value) -> bool;

  auto kenwood_power_get() -> PowerLevel;
  auto kenwood_power_set(PowerLevel val) -> bool;

  auto kenwood_menu_get(int menu_num) -> int;
  auto kenwood_menu_set(int menu_num, int value) -> bool;

  UsbOutSelect orig_menu_102_{UsbOutSelect::unknown};
  vfo_t orig_vfo_{RIG_VFO_NONE};

  freq_t orig_other_frequency_{0};
  rmode_t orig_other_mode_{RIG_MODE_NONE};
  PowerLevel orig_other_power_{PowerLevel::UNKNOWN};
  bool orig_other_freq_saved_{false};
  bool orig_other_mode_saved_{false};
  bool orig_other_power_saved_{false};
};

#endif // THD75_HPP

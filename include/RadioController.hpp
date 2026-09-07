#ifndef RADIOCONTROLLER_HPP
#define RADIOCONTROLLER_HPP

#include <filesystem>
#include <fstream>
#include <functional>
#include <hamlib/rig.h>
#include <matvec.hpp>
#include <matvec/tcd.hpp>
#include <memory>
#include <pillow.hpp>
#include <pillow/pillow.hpp>
#include <set>
#include <string>
#include <sys/socket.h>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

class RadioController {
public:
  enum class PowerLevel {
    HIGH = 0,      ///< High power (maximum TX power)
    MEDIUM = 1,    ///< Medium power
    LOW = 2,       ///< Low power
    EXTRA_LOW = 3, ///< Extra low power
    UNKNOWN = -1   ///< Unknown or uninitialized power level
  };

  enum class UsbOutSelect {
    AF = 0,     ///< Audio Frequency output (demodulated audio)
    IF = 1,     ///< Intermediate Frequency output (raw signal)
    Detect = 2, ///< Detection signal
    unknown     ///< Unknown or undefined value
  };

  enum class Mode : rmode_t {
    FM = RIG_MODE_FM,   ///< FM mode
    WFM = RIG_MODE_WFM, ///< Wide FM mode
    AM = RIG_MODE_AM,   ///< AM mode
    USB = RIG_MODE_USB, ///< Upper Sideband
    LSB = RIG_MODE_LSB, ///< Lower Sideband
    CW = RIG_MODE_CW,   ///< Morse code
    CWR = RIG_MODE_CWR, ///< CW reverse
    DD = RIG_MODE_DD    ///< Digital voice
  };

  RadioController(rig_model_t model, std::string port,
                  bool hamlib_debug = false);

  virtual void shutdown();

  virtual ~RadioController();

  virtual bool initialize();

  virtual bool set_frequency(double freq_mhz);

  virtual bool get_frequency(double &freq_mhz);

  virtual bool get_mode(Mode &mode);

  virtual bool set_mode(Mode mode);

  virtual bool set_power_level(const std::string &level);

  virtual bool get_power_level(std::string &level);

  virtual bool set_ptt(bool transmit);

  virtual bool get_dcd(bool &is_squelch_open);

  void flush_serial();

  static std::vector<std::string> find_tty_sysfs() {
    return find_tty_sysfs(0x2166, 0x9023);
  }

  std::string find_alsa_device() { return find_alsa_device(port_); }

protected:
  static std::vector<std::string> find_tty_sysfs(unsigned int target_vid,
                                                 unsigned int target_pid);

  static std::string find_alsa_device(const std::string &port);

  static std::string read_sysfs_attr(const fs::path &filepath);

  rig_model_t model_; ///< Hamlib rig model
  std::string port_;  ///< Serial port path

protected:
  RIG *rig_;          ///< Hamlib rig handle
  Mode current_mode_; ///< Current radio mode (tracked for compatibility)

  // Backup variables for restoring radio state
  rmode_t orig_mode_;         ///< Saved original radio mode
  bool orig_mode_saved_;      ///< Flag indicating if mode was saved
  freq_t orig_frequency_;     ///< Saved original frequency (Hz)
  bool orig_frequency_saved_; ///< Flag indicating if frequency was saved

  pbwidth_t orig_width_;  ///< Saved original bandwidth
  PowerLevel orig_power_; ///< Saved original power level
  bool orig_power_saved_; ///< Flag indicating if power level was saved
};

#endif // RADIOCONTROLLER_HPP

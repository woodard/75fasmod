#pragma once // NOLINT(llvm-header-guard,cppcoreguidelines-header-guard)
#include <gnuradio/digital/constellation.h>
#include <gnuradio/trellis/fsm.h>

enum class ModulationScheme {
  QAM16,  ///< 16-QAM (4 bits per symbol)
  QAM32,  ///< 32-QAM (5 bits per symbol)
  QAM64,  ///< 64-QAM (6 bits per symbol)
  QAM128, ///< 128-QAM (7 bits per symbol)
  QAM256  ///< 256-QAM (8 bits per symbol)
};

class TcmConfig {
public:
  static auto get_fsm(ModulationScheme scheme) -> gr::trellis::fsm;

  static auto get_constellation(ModulationScheme scheme)
      -> gr::digital::constellation_sptr;
};

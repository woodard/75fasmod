#pragma once
#include <gnuradio/digital/constellation.h>
#include <gnuradio/trellis/fsm.h>
#include <vector>

enum class ModulationScheme {
  QAM16,
  QAM32,
  QAM64,
  QAM128,
  QAM256
};

class TcmConfig {
public:
  // Returns the gr-trellis FSM for the requested scheme
  static gr::trellis::fsm get_fsm(ModulationScheme scheme);

  // Generates custom QAM grid points (including cross-constellations)
  static gr::digital::constellation_sptr get_constellation(ModulationScheme scheme);
};

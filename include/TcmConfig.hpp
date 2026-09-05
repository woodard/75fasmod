/**
 * @file TcmConfig.hpp
 * @brief TCM (Trellis Coded Modulation) configuration
 *
 * This module provides utilities for configuring Trellis Coded Modulation
 * and QAM (Quadrature Amplitude Modulation) constellations for the 75fasmod
 * modem. It uses GNU Radio's trellis and digital modules to generate
 * the necessary Signal Processing objects.
 *
 * TCM combines convolutional coding with constellation modulation to
 * achieve higher data rates while maintaining error correction capability.
 */

#pragma once
#include <gnuradio/digital/constellation.h>
#include <gnuradio/trellis/fsm.h>
#include <vector>

/**
 * @brief Enumeration of supported modulation schemes
 *
 * Defines the available modulation orders for the modem.
 * Higher orders provide higher data rates but require better SNR.
 */
enum class ModulationScheme {
  QAM16,  ///< 16-QAM (4 bits per symbol)
  QAM32,  ///< 32-QAM (5 bits per symbol)
  QAM64,  ///< 64-QAM (6 bits per symbol)
  QAM128, ///< 128-QAM (7 bits per symbol)
  QAM256  ///< 256-QAM (8 bits per symbol)
};

/**
 * @brief TCM configuration utility class
 *
 * Provides static methods to generate the GNU Radio objects needed
 * for TCM modulation and demodulation. These include finite state
 * machines (FSMs) for the convolutional encoder and constellation
 * mappings for QAM modulation.
 */
class TcmConfig {
public:
  /**
   * @brief Gets the gr-trellis FSM for the requested scheme
   *
   * Generates a finite state machine (FSM) for the convolutional
   * encoder used in TCM. The FSM defines the state transitions
   * and output symbols for the chosen modulation scheme.
   *
   * @param scheme Modulation scheme (QAM16, QAM32, etc.)
   * @return gr::trellis::fsm Finite state machine for the encoder
   */
  static gr::trellis::fsm get_fsm(ModulationScheme scheme);

  /**
   * @brief Generates custom QAM grid points
   *
   * Creates a constellation mapping with cross-constellations
   * optimized for the chosen modulation scheme. The constellations
   * are designed for TCM operation.
   *
   * @param scheme Modulation scheme (QAM16, QAM32, etc.)
   * @return gr::digital::constellation_sptr Constellation object
   */
  static gr::digital::constellation_sptr
  get_constellation(ModulationScheme scheme);
};

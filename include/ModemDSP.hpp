/**
 * @file ModemDSP.hpp
 * @brief Digital Signal Processing for 75fasmod modem
 *
 * This module provides the ModemDSP class that interfaces with GNU Radio
 * for digital signal processing in the 75fasmod modem. It manages the
 * GNU Radio top-block configuration for both receiver and transmitter
 * chains, connecting to ALSA audio devices for hardware interfacing.
 *
 * The modem uses advanced modulation techniques (Adaptive QAM and TCM)
 * to achieve high data rates on FM radio links.
 */

#pragma once
#include <cstdint>
#include <gnuradio/top_block.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Digital Signal Processing manager for 75fasmod
 *
 * This class manages the GNU Radio signal processing chains for the modem.
 * It handles receiver and transmitter setup, connecting to ALSA audio
 * devices for hardware I/O.
 *
 * Key responsibilities:
 * - Configure GNU Radio flowgraphs for RX and TX
 * - Manage ALSA audio device connections
 * - Provide simple start/stop interfaces for DSP streams
 */
class ModemDSP {
public:
  /**
   * @brief Constructs a new ModemDSP object
   *
   * @param alsa_tx_device ALSA device path for transmission (e.g.,
   * "plughw:0,0")
   * @param alsa_rx_device ALSA device path for reception (e.g., "plughw:0,0")
   */
  ModemDSP(const std::string &alsa_tx_device,
           const std::string &alsa_rx_device);

  /**
   * @brief Destructor
   *
   * Cleans up GNU Radio resources and closes ALSA connections.
   */
  ~ModemDSP();

  /**
   * @brief Starts receiver processing
   *
   * Initializes and starts the GNU Radio receiver flowgraph.
   *
   * @param output_fd File descriptor to write decoded data to
   * @return true if started successfully, false otherwise
   */
  bool start_rx(int output_fd);

  /**
   * @brief Stops receiver processing
   *
   * Stops the receiver flowgraph and releases resources.
   */
  void stop_rx();

  /**
   * @brief Starts transmitter processing
   *
   * Initializes and starts the GNU Radio transmitter flowgraph.
   *
   * @param input_fd File descriptor to read data from
   * @return true if started successfully, false otherwise
   */
  bool start_tx(int input_fd);

  /**
   * @brief Stops transmitter processing
   *
   * Stops the transmitter flowgraph and releases resources.
   */
  void stop_tx();

private:
  std::string alsa_tx_device_;
  std::string alsa_rx_device_;
  int tx_pipe_fd_ = -1;
  gr::top_block_sptr rx_tb_; ///< GNU Radio receiver top block
  gr::top_block_sptr tx_tb_; ///< GNU Radio transmitter top block
};

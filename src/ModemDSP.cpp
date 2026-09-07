#include "ModemDSP.hpp"

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

/**
 * @brief Constructs a new ModemDSP object
 *
 * @param alsa_tx_device ALSA device path for transmission (e.g.,
 * "plughw:0,0")
 * @param alsa_rx_device ALSA device path for reception (e.g., "plughw:0,0")
 */

/**
 * @brief Destructor
 *
 * Cleans up GNU Radio resources and closes ALSA connections.
 */

/**
 * @brief Starts receiver processing
 *
 * Initializes and starts the GNU Radio receiver flowgraph.
 *
 * @param output_fd File descriptor to write decoded data to
 * @return true if started successfully, false otherwise
 */

/**
 * @brief Stops receiver processing
 *
 * Stops the receiver flowgraph and releases resources.
 */

/**
 * @brief Starts transmitter processing
 *
 * Initializes and starts the GNU Radio transmitter flowgraph.
 *
 * @param input_fd File descriptor to read data from
 * @return true if started successfully, false otherwise
 */

/**
 * @brief Stops transmitter processing
 *
 * Stops the transmitter flowgraph and releases resources.
 */

#include "TcmConfig.hpp"
#include <gnuradio/audio/sink.h>
#include <gnuradio/audio/source.h>
#include <gnuradio/blocks/complex_to_real.h>
#include <gnuradio/blocks/file_descriptor_sink.h>
#include <gnuradio/blocks/file_descriptor_source.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/repack_bits_bb.h>
#include <gnuradio/digital/chunks_to_symbols.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/interp_fir_filter.h>
#include <gnuradio/top_block.h>
#include <gnuradio/trellis/encoder.h>
#include <iostream>
#include <stdexcept> // Ensure this is included at the top for std::exception
#include <unistd.h>

namespace {
constexpr int ALSA_SAMPLE_RATE = 48000;
constexpr int BITS_PER_BYTE = 8;
constexpr int CHIPS_PER_SYMBOL = 5;
constexpr float RRC_ROLLOFF = 0.35F;
constexpr int RRC_FILTER_TAPS_MULTIPLIER = 11;
constexpr float SIGNAL_AMPLITUDE = 0.5F;
} // namespace

ModemDSP::ModemDSP(std::string alsa_tx_device, std::string alsa_rx_device)
    : alsa_tx_device_(std::move(alsa_tx_device)),
      alsa_rx_device_(std::move(alsa_rx_device)) {}

ModemDSP::~ModemDSP() {
  stop_rx();
  stop_tx();
}

auto ModemDSP::start_rx(int output_fd) -> bool {
  rx_tb_ = gr::make_top_block("rx_continuous_flowgraph");

  try {
    auto audio_src = gr::audio::source::make(ALSA_SAMPLE_RATE, alsa_rx_device_);
    auto fd_sink =
        gr::blocks::file_descriptor_sink::make(sizeof(uint8_t), output_fd);

    rx_tb_->start();
    std::cout << "[DSP] Continuous RX Flowgraph started.\n";
    return true;
  } catch (const std::exception &e) {
    std::cerr << "\n[DSP] CRITICAL ERROR in start_rx initializing ALSA device '"
              << alsa_rx_device_ << "':\n -> " << e.what() << "\n\n";
    return false;
  }
}

void ModemDSP::stop_rx() {
  if (rx_tb_) {
    rx_tb_->stop();
    rx_tb_->wait();
    rx_tb_.reset();
  }
}

auto ModemDSP::start_tx(int input_fd) -> bool {
  tx_pipe_fd_ = input_fd;
  tx_tb_ = gr::make_top_block("tx_continuous_flowgraph");

  auto fsm = TcmConfig::get_fsm(ModulationScheme::QAM16);
  auto qam = TcmConfig::get_constellation(ModulationScheme::QAM16);

  int sps = CHIPS_PER_SYMBOL;
  float rolloff = RRC_ROLLOFF;

  try {
    auto src = gr::blocks::file_descriptor_source::make(sizeof(uint8_t),
                                                        input_fd, false);
    auto repack = gr::blocks::repack_bits_bb::make(BITS_PER_BYTE, 3);
    auto trellis_encoder =
        gr::trellis::encoder<uint8_t, uint8_t>::make(fsm, 0, 0);
    auto mapper = gr::digital::chunks_to_symbols<uint8_t, gr_complex>::make(
        qam->points());

    std::vector<float> rrc_taps = gr::filter::firdes::root_raised_cosine(
        sps, sps, 1.0, rolloff, RRC_FILTER_TAPS_MULTIPLIER * sps);
    auto rrc_filter = gr::filter::interp_fir_filter_ccf::make(sps, rrc_taps);

    auto complex_to_real = gr::blocks::complex_to_real::make(1);
    auto gain = gr::blocks::multiply_const_ff::make(SIGNAL_AMPLITUDE);
    auto sink = gr::audio::sink::make(ALSA_SAMPLE_RATE, alsa_tx_device_, true);

    tx_tb_->connect(src, 0, repack, 0);
    tx_tb_->connect(repack, 0, trellis_encoder, 0);
    tx_tb_->connect(trellis_encoder, 0, mapper, 0);
    tx_tb_->connect(mapper, 0, rrc_filter, 0);
    tx_tb_->connect(rrc_filter, 0, complex_to_real, 0);
    tx_tb_->connect(complex_to_real, 0, gain, 0);
    tx_tb_->connect(gain, 0, sink, 0);

    tx_tb_->start();
    std::cout << "[DSP] Continuous TX Flowgraph started.\n";
    return true;
  } catch (const std::exception &e) {
    std::cerr << "\n[DSP] CRITICAL ERROR in start_tx initializing ALSA device '"
              << alsa_tx_device_ << "':\n -> " << e.what() << "\n\n";
    return false;
  }
}

void ModemDSP::stop_tx() {
  if (tx_tb_) {
    tx_tb_->stop();
    tx_tb_->wait();
    tx_tb_.reset();
  }
}

#include "ModemDSP.hpp"
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

ModemDSP::ModemDSP(const std::string &alsa_tx_device,
                   const std::string &alsa_rx_device)
    : alsa_tx_device_(alsa_tx_device), alsa_rx_device_(alsa_rx_device) {}

ModemDSP::~ModemDSP() {
  stop_rx();
  stop_tx();
}

bool ModemDSP::start_rx(int output_fd) {
  rx_tb_ = gr::make_top_block("rx_continuous_flowgraph");

  try {
    auto audio_src = gr::audio::source::make(48000, alsa_rx_device_);
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

bool ModemDSP::start_tx(int input_fd) {
  tx_pipe_fd_ = input_fd;
  tx_tb_ = gr::make_top_block("tx_continuous_flowgraph");

  auto fsm = TcmConfig::get_fsm(ModulationScheme::QAM16);
  auto qam = TcmConfig::get_constellation(ModulationScheme::QAM16);

  int sps = 5;
  float rolloff = 0.35;

  try {
    auto src = gr::blocks::file_descriptor_source::make(sizeof(uint8_t),
                                                        input_fd, false);
    auto repack = gr::blocks::repack_bits_bb::make(8, 3);
    auto trellis_encoder =
        gr::trellis::encoder<uint8_t, uint8_t>::make(fsm, 0, 0);
    auto mapper = gr::digital::chunks_to_symbols<uint8_t, gr_complex>::make(
        qam->points());

    std::vector<float> rrc_taps = gr::filter::firdes::root_raised_cosine(
        sps, sps, 1.0, rolloff, 11 * sps);
    auto rrc_filter = gr::filter::interp_fir_filter_ccf::make(sps, rrc_taps);

    auto complex_to_real = gr::blocks::complex_to_real::make(1);
    auto gain = gr::blocks::multiply_const_ff::make(0.5);
    auto sink = gr::audio::sink::make(48000, alsa_tx_device_, true);

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

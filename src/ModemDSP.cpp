#include "ModemDSP.hpp"
#include "TcmConfig.hpp"
#include <gnuradio/audio/sink.h>
#include <gnuradio/audio/source.h>
#include <gnuradio/blocks/complex_to_real.h>
#include <gnuradio/blocks/file_descriptor_sink.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/repack_bits_bb.h>
#include <gnuradio/blocks/vector_source.h>
#include <gnuradio/digital/chunks_to_symbols.h>
#include <gnuradio/digital/constellation.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/interp_fir_filter.h>
#include <gnuradio/trellis/encoder.h>
#include <gnuradio/top_block.h>
#include <iostream>

ModemDSP::~ModemDSP() { stop_rx(); }

void ModemDSP::start_rx(int output_fd) {
  rx_tb_ = gr::make_top_block("rx_continuous_flowgraph");

  // 1. Audio Source (ALSA)
  auto audio_src = gr::audio::source::make(48000, alsa_device_);

  // 2. File Descriptor Sink (Writes decoded bytes directly to C++ pipe)
  auto fd_sink =
      gr::blocks::file_descriptor_sink::make(sizeof(uint8_t), output_fd);

  rx_tb_->start();
  std::cout << "[DSP] Continuous RX Flowgraph started.\n";
}

void ModemDSP::stop_rx() {
  if (rx_tb_) {
    rx_tb_->stop();
    rx_tb_->wait();
    rx_tb_.reset();
  }
}

void ModemDSP::transmit_burst(const std::vector<uint8_t> &framed_data) {
  // 1. Create the Top Block
  auto tb = gr::make_top_block("tx_burst");

  // 2. Prepend Training Sequence (Preamble)
  std::vector<uint8_t> payload_with_preamble;
  for (int i = 0; i < 16; ++i) { // 16 bytes of preamble
    payload_with_preamble.push_back(0xAA);
  }
  payload_with_preamble.insert(payload_with_preamble.end(), framed_data.begin(),
                               framed_data.end());

  // 3. Define the Constellation and Trellis FSM
  auto fsm = TcmConfig::get_fsm(ModulationScheme::QAM16);
  auto qam = TcmConfig::get_constellation(ModulationScheme::QAM16);

  // 4. Instantiate the DSP Blocks
  int sps = 5; // Samples Per Symbol (9600 baud * 5 sps = 48000 Hz sample rate)
  float rolloff = 0.35; // Filter alpha (excess bandwidth)

  auto src = gr::blocks::vector_source_b::make(payload_with_preamble, false);

  // Repacker: Slices 8-bit bytes into 3-bit informational chunks (k=3)
  auto repack = gr::blocks::repack_bits_bb::make(8, 3);

  // Trellis Encoder: Applies convolutional code + set partitioning
  auto trellis_encoder = gr::trellis::encoder<uint8_t, uint8_t>::make(fsm, 0, 0);

  // Symbol Mapper: Maps Trellis indices (0-15) directly to complex QAM coordinates
  auto mapper = gr::digital::chunks_to_symbols<uint8_t, gr_complex>::make(qam->points());

  // RRC Filter
  std::vector<float> rrc_taps = gr::filter::firdes::root_raised_cosine(
      sps, sps, 1.0, rolloff, 11 * sps);
  auto rrc_filter = gr::filter::interp_fir_filter_ccf::make(sps, rrc_taps);

  auto complex_to_real = gr::blocks::complex_to_real::make(1);
  auto gain = gr::blocks::multiply_const_ff::make(0.5);
  auto sink = gr::audio::sink::make(48000, alsa_device_, true);

  // 5. Connect the Flowgraph
  tb->connect(src, 0, repack, 0);
  tb->connect(repack, 0, trellis_encoder, 0);
  tb->connect(trellis_encoder, 0, mapper, 0);
  tb->connect(mapper, 0, rrc_filter, 0);
  tb->connect(rrc_filter, 0, complex_to_real, 0);
  tb->connect(complex_to_real, 0, gain, 0);
  tb->connect(gain, 0, sink, 0);

  // 6. Execute the Burst
  std::cout << "[DSP] Transmitting TCM burst (" << payload_with_preamble.size()
            << " bytes)...\n";
  tb->start();
  tb->wait();
  std::cout << "[DSP] TCM Burst complete.\n";
}

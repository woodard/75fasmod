#include "ModemDSP.hpp"
#include <gnuradio/audio/sink.h>
#include <gnuradio/audio/source.h>
#include <gnuradio/blocks/complex_to_real.h>
#include <gnuradio/blocks/file_descriptor_sink.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/vector_source.h>
#include <gnuradio/digital/constellation.h>
#include <gnuradio/digital/constellation_decoder_cb.h>
#include <gnuradio/digital/constellation_encoder_bc.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/interp_fir_filter.h>
#include <gnuradio/top_block.h>
#include <iostream>

ModemDSP::ModemDSP() {}

ModemDSP::~ModemDSP() { stop_rx(); }

void ModemDSP::start_rx(int output_fd) {
  rx_tb_ = gr::make_top_block("rx_continuous_flowgraph");

  // 1. Audio Source (ALSA)
  auto audio_src = gr::audio::source::make(48000, "hw:CARD=THD75,DEV=0");

  // [DSP PLACEHOLDER]: Real->Complex, AGC, Clock Recovery, LMS Equalizer, and
  // Constellation Decoder go here. To keep it compiling before we write the
  // heavy math, we wire it up as a passthrough placeholder. In reality, you'd
  // feed the output of your Viterbi decoder / QAM slicer into this file
  // descriptor sink.

  // 2. File Descriptor Sink (Writes decoded bytes directly to our C++ pipe)
  auto fd_sink =
      gr::blocks::file_descriptor_sink::make(sizeof(uint8_t), output_fd);

  // rx_tb_->connect(audio_src, 0, dsp_magic, 0);
  // rx_tb_->connect(dsp_magic, 0, fd_sink, 0);

  // Start in the background (non-blocking)
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
  // A robust, alternating sequence (e.g., 0xAA = 10101010) gives the Costas
  // Loop and LMS Equalizer a strong, predictable edge to lock onto quickly.
  std::vector<uint8_t> payload_with_preamble;
  for (int i = 0; i < 16; ++i) { // 16 bytes of preamble
    payload_with_preamble.push_back(0xAA);
  }
  // Append the actual MAC frames payload after the preamble
  payload_with_preamble.insert(payload_with_preamble.end(), framed_data.begin(),
                               framed_data.end());

  // 3. Define the Constellation (16-QAM)
  // (If you already declare 'qam' as a class member, you can safely remove this
  // line)
  auto qam = gr::digital::constellation_16qam::make();

  // 4. Instantiate the DSP Blocks
  int sps = 5; // Samples Per Symbol (9600 baud * 4 sps = 38400 Hz sample rate)
  float rolloff = 0.35; // Filter alpha (excess bandwidth)

  // Source: Reads our bytes exactly once (repeat = false) and stops
  auto src = gr::blocks::vector_source_b::make(payload_with_preamble, false);

  // Encoder: Maps bytes onto the complex 2D QAM grid
  auto encoder = gr::digital::constellation_encoder_bc::make(qam);

  // RRC Filter: Interpolates sudden symbol jumps into smooth, contained
  // waveforms
  std::vector<float> rrc_taps = gr::filter::firdes::root_raised_cosine(
      sps,     // Gain
      sps,     // Sampling freq (normalized to sps)
      1.0,     // Symbol rate (normalized)
      rolloff, // Rolloff factor
      11 * sps // Number of taps (11 symbols wide for clean overlap)
  );
  auto rrc_filter = gr::filter::interp_fir_filter_ccf::make(sps, rrc_taps);

  // Converter: Discard the Q channel and pass only the real part to the
  // soundcard
  auto complex_to_real = gr::blocks::complex_to_real::make(1);

  // Volume/Gain: Scale the float values (-1.0 to 1.0) so ALSA doesn't clip.
  // 0.5 is a safe starting point. If your transmitted audio is too quiet, raise
  // this.
  auto gain = gr::blocks::multiply_const_ff::make(0.5);

  // Audio Sink: Send to ALSA.
  // An empty string "" tells GNU Radio to use the system default soundcard.
  auto sink = gr::audio::sink::make(38400, "", true);

  // 5. Connect the Flowgraph
  tb->connect(src, 0, encoder, 0);
  tb->connect(encoder, 0, rrc_filter, 0);
  tb->connect(rrc_filter, 0, complex_to_real, 0);
  tb->connect(complex_to_real, 0, gain, 0);
  tb->connect(gain, 0, sink, 0);

  // 6. Execute the Burst
  std::cout << "[DSP] Transmitting burst (" << payload_with_preamble.size()
            << " bytes)...\n";
  tb->start();
  tb->wait(); // Wait blocks the thread until the vector_source runs out of
              // bytes
  std::cout << "[DSP] Burst complete.\n";
}

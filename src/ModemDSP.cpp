#include "ModemDSP.hpp"
#include <gnuradio/audio/source.h>
#include <gnuradio/blocks/file_descriptor_sink.h>
#include <gnuradio/digital/constellation_decoder.h>
#include <gnuradio/blocks/vector_source_b.h>
#include <gnuradio/blocks/complex_to_real.h>
#include <gnuradio/blocks/multiply_const_ff.h>
#include <gnuradio/digital/constellation_modulator.h>
#include <gnuradio/digital/constellation.h>
#include <gnuradio/audio/sink.h>
#include <iostream>

ModemDSP::ModemDSP() {}

ModemDSP::~ModemDSP() {
    stop_rx();
}

void ModemDSP::start_rx(int output_fd) {
    rx_tb_ = gr::make_top_block("rx_continuous_flowgraph");

    // 1. Audio Source (ALSA)
    auto audio_src = gr::audio::source::make(48000, "hw:CARD=THD75,DEV=0");

    // [DSP PLACEHOLDER]: Real->Complex, AGC, Clock Recovery, LMS Equalizer, and Constellation Decoder go here.
    // To keep it compiling before we write the heavy math, we wire it up as a passthrough placeholder.
    // In reality, you'd feed the output of your Viterbi decoder / QAM slicer into this file descriptor sink.
    
    // 2. File Descriptor Sink (Writes decoded bytes directly to our C++ pipe)
    auto fd_sink = gr::blocks::file_descriptor_sink::make(sizeof(uint8_t), output_fd);

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

void ModemDSP::transmit_burst(const std::vector<uint8_t>& framed_data) {
    // 1. Create a fresh Top Block for this burst
    auto tb = gr::make_top_block("tx_burst_flowgraph");

    // 2. Vector Source: Feeds our COBS encoded bytes. 'false' means don't repeat.
    // When the vector is empty, it sends a DONE signal, and tb->wait() will return.
    auto src = gr::blocks::vector_source_b::make(framed_data, false);

    // 3. QAM Modulator 
    // Uses 16-QAM as a base. (TCM requires an FSM passed to gr::trellis::encoder here).
    auto qam = gr::digital::constellation_16qam::make();
    
    // constellation_modulator parameters: constellation, diff_encode, samples_per_symbol, excess_bw
    auto mod = gr::digital::constellation_modulator::make(qam->base(), false, 4, 0.35);

    // 4. Baseband to Real / IF Conversion
    // Modulators output Complex (I/Q) baseband signals, but the radio needs real audio.
    // For a true FM passband, this complex signal should be mixed with an audio IF (e.g., 1500Hz).
    // For this boilerplate, we'll simply extract the real component to allow audio sink mapping.
    auto c2r = gr::blocks::complex_to_real::make(1);
    
    // 5. Volume Attenuation (Prevent ALSA clipping)
    auto vol = gr::blocks::multiply_const_ff::make(0.5f);

    // 6. Audio Sink (48kHz ALSA)
    auto audio_snk = gr::audio::sink::make(48000, "hw:CARD=THD75,DEV=0");

    // 7. Connect the blocks
    tb->connect(src, 0, mod, 0);
    tb->connect(mod, 0, c2r, 0);
    tb->connect(c2r, 0, vol, 0);
    tb->connect(vol, 0, audio_snk, 0);

    // 8. Execute Flowgraph
    std::cout << "[DSP] Starting GNU Radio flowgraph...\n";
    tb->start();
    
    // Blocks the thread until the vector_source_b finishes streaming the bytes
    tb->wait(); 
    std::cout << "[DSP] Flowgraph execution complete.\n";
}

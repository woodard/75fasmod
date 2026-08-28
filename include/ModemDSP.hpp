#pragma once
#include <cstdint>
#include <gnuradio/top_block.h>
#include <memory>
#include <vector>

class ModemDSP {
public:
  ModemDSP();
  ~ModemDSP();

  void start_rx(int output_fd); // Starts the continuous RX pipeline
  void stop_rx();
  // Takes framed data, runs it through the GR modulator, and pushes to ALSA
  void transmit_burst(const std::vector<uint8_t> &framed_data);

private:
  gr::top_block_sptr rx_tb_;
};

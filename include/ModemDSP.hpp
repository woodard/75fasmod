#pragma once
#include <cstdint>
#include <gnuradio/top_block.h>
#include <memory>
#include <vector>

class ModemDSP {
public:
  ModemDSP(const std::string& alsa_device) : alsa_device_(alsa_device) {}
  ~ModemDSP();

  void start_rx(int output_fd); // Starts the continuous RX pipeline
  void stop_rx();
  // Takes framed data, runs it through the GR modulator, and pushes to ALSA
  void transmit_burst(const std::vector<uint8_t> &framed_data);

private:
  std::string alsa_device_;                 // Store the device string
  gr::top_block_sptr rx_tb_;
};

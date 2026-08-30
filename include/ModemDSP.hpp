#pragma once
#include <cstdint>
#include <gnuradio/top_block.h>
#include <memory>
#include <string>
#include <vector>

class ModemDSP {
public:
  ModemDSP(const std::string& alsa_tx_device, const std::string& alsa_rx_device);
  ~ModemDSP();

  bool start_rx(int output_fd);
  void stop_rx();

  bool start_tx(int input_fd);
  void stop_tx();

private:
  std::string alsa_tx_device_;
  std::string alsa_rx_device_;
  int tx_pipe_fd_ = -1;
  gr::top_block_sptr rx_tb_;
  gr::top_block_sptr tx_tb_;
};

#pragma once // NOLINT(llvm-header-guard,cppcoreguidelines-header-guard)
#include <gnuradio/top_block.h>

class ModemDSP {
public:
  ModemDSP(std::string alsa_tx_device, std::string alsa_rx_device);

  ~ModemDSP();

  auto start_rx(int output_fd) -> bool;

  void stop_rx();

  auto start_tx(int input_fd) -> bool;

  void stop_tx();

private:
  std::string alsa_tx_device_;
  std::string alsa_rx_device_;
  int tx_pipe_fd_ = -1;
  gr::top_block_sptr rx_tb_; ///< GNU Radio receiver top block
  gr::top_block_sptr tx_tb_; ///< GNU Radio transmitter top block
};

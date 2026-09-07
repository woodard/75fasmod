#ifndef DATASOCKET_HPP
#define DATASOCKET_HPP

#pragma once

// Necessary includes for std:: types used in this header
#include <chrono>
#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Forward declarations
class RadioController;
class ModemDSP;

class DataSocket {
public:
  static constexpr int DEFAULT_BURST_LIMIT = 8;
  static constexpr int DEFAULT_FLUSH_TIMEOUT_MS = 200;

  DataSocket(std::string socket_path, RadioController &radio, ModemDSP &dsp,
             int burst_limit = DEFAULT_BURST_LIMIT,
             int flush_timeout_ms = DEFAULT_FLUSH_TIMEOUT_MS);

  ~DataSocket();

  auto start() -> bool;

  void stop();

private:
  void accept_loop(std::stop_token stoken);
  void handle_client(int client_fd, std::stop_token &stoken);

  std::string socket_path_;
  int server_fd_;
  std::jthread worker_thread_;

  RadioController &radio_;
  ModemDSP &dsp_;

  int burst_limit_;
  int flush_timeout_ms_;

  int rx_pipe_[2];
  int tx_pipe_[2];
  int active_client_fd_;

  std::vector<std::vector<uint8_t>> tx_queue_;
  std::chrono::steady_clock::time_point queue_start_time_;

  // Non-blocking MAC State Tracking
  bool is_transmitting_ = false;
  std::chrono::steady_clock::time_point ptt_drop_time_;
  std::chrono::steady_clock::time_point tx_resume_time_; // MAC Cooldown
  int frames_sent_in_burst_ = 0;
};

#endif

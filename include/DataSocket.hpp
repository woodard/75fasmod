#ifndef DATASOCKET_HPP
#define DATASOCKET_HPP

/**
 * @file DataSocket.hpp
 * @brief IPC socket server for 75fasmod modem data transfer
 *
 * This module provides a socket-based IPC (Inter-Process Communication)
 * server that allows external processes to connect to the 75fasmod modem
 * and exchange data with it. The server manages burst transmission
 * limits and handles multiple client connections.
 */

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

/**
 * @brief Socket server for modem data transfer
 *
 * This class implements a Unix domain socket server that listens for
 * connections from client processes. It manages data transfer between
 * the modem and connected clients, handling burst transmission limits
 * and PTT (Push-To-Talk) control.
 *
 * Key features:
 * - Background thread for accepting connections
 * - Burst transmission limiting to prevent radio overload
 * - Automatic PTT control during transmission
 * - Thread-safe queue management
 */
class DataSocket {
public:
  static constexpr int DEFAULT_BURST_LIMIT = 8;
  static constexpr int DEFAULT_FLUSH_TIMEOUT_MS = 200;

  /**
   * @brief Constructs a new DataSocket object
   *
   * @param socket_path Path for the Unix domain socket
   * @param radio Reference to RadioController for PTT control
   * @param dsp Reference to ModemDSP for signal processing
   * @param burst_limit Maximum frames to transmit in a single burst
   * @param flush_timeout_ms Timeout in milliseconds to flush the transmit queue
   */
  DataSocket(std::string socket_path, RadioController &radio,
             ModemDSP &dsp, int burst_limit = DEFAULT_BURST_LIMIT,
             int flush_timeout_ms = DEFAULT_FLUSH_TIMEOUT_MS);

  /**
   * @brief Destructor
   *
   * Stops the server thread and cleans up resources.
   */
  ~DataSocket();

  /**
   * @brief Starts the background server thread
   *
   * Starts a background thread to listen for IPC connections.
   *
   * @return true if thread started successfully, false otherwise
   */
  auto start() -> bool;

  /**
   * @brief Stops the server thread and cleans up
   *
   * Signals the server thread to stop and waits for it to complete.
   * Removes the socket file.
   */
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

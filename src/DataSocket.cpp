#include "DataSocket.hpp"
#include "Frame.hpp"
#include "ModemDSP.hpp"
#include "RadioController.hpp"
#include "cobs.hpp"
#include "crc32.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

DataSocket::DataSocket(const std::string &socket_path, RadioController &radio,
                       ModemDSP &dsp, int burst_limit, int flush_timeout_ms)
    : socket_path_(socket_path), server_fd_(-1), radio_(radio), dsp_(dsp),
      burst_limit_(burst_limit), flush_timeout_ms_(flush_timeout_ms) {
  rx_pipe_[0] = -1;
  rx_pipe_[1] = -1;
  active_client_fd_ = -1;
}

DataSocket::~DataSocket() { stop(); }

bool DataSocket::start() {
  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    std::cerr << "Error: Could not create Unix socket.\n";
    return false;
  }

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

  unlink(socket_path_.c_str());

  if (bind(server_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "Error: Could not bind Unix socket to " << socket_path_
              << "\n";
    close(server_fd_);
    return false;
  }

  if (listen(server_fd_, 5) < 0) {
    std::cerr << "Error: Could not listen on Unix socket.\n";
    close(server_fd_);
    return false;
  }

  // Create the POSIX Pipe for continuous RX data
  if (pipe(rx_pipe_) < 0) {
    std::cerr << "Error: Could not create RX pipe.\n";
    return false;
  }

  // Start the continuous DSP RX flowgraph, giving it the write-end of the pipe
  dsp_.start_rx(rx_pipe_[1]);

  worker_thread_ = std::jthread(
      [this](std::stop_token stoken) { this->accept_loop(stoken); });

  std::cout << "Data socket listening on " << socket_path_ << "\n";

  return true;
}

void DataSocket::stop() {
  if (worker_thread_.joinable()) {
    worker_thread_.request_stop();
    worker_thread_.join();
  }

  dsp_.stop_rx();

  if (rx_pipe_[0] >= 0) {
    close(rx_pipe_[0]);
    rx_pipe_[0] = -1;
  }
  if (rx_pipe_[1] >= 0) {
    close(rx_pipe_[1]);
    rx_pipe_[1] = -1;
  }

  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
    unlink(socket_path_.c_str());
  }
}

void DataSocket::accept_loop(std::stop_token stoken) {
  struct pollfd pfd{};
  pfd.fd = server_fd_;
  pfd.events = POLLIN;

  while (!stoken.stop_requested()) {
    int ret = poll(&pfd, 1, 100);
    if (ret > 0 && (pfd.revents & POLLIN)) {
      int client_fd = accept(server_fd_, nullptr, nullptr);
      if (client_fd >= 0) {
        std::cout << "Client connected to data socket.\n";
        handle_client(client_fd, stoken);
        std::cout << "Client disconnected.\n";
        close(client_fd);
      }
    }
  }
}

void DataSocket::handle_client(int client_fd, std::stop_token &stoken) {
  active_client_fd_ = client_fd;

  struct pollfd pfds[2];
  pfds[0].fd = client_fd;
  pfds[0].events = POLLIN;
  pfds[1].fd = rx_pipe_[0];
  pfds[1].events = POLLIN;

  std::vector<uint8_t> rx_buffer(2048);
  std::vector<uint8_t> rx_stream_accum; // Accumulates continuous DSP bytes
  uint8_t current_seq = 0;

  tx_queue_.clear();

  while (!stoken.stop_requested()) {
    int ret = poll(pfds, 2, 10);
    auto now = std::chrono::steady_clock::now();

    if (ret > 0) {
      // =================================================================
      // EVENT 1: TX PIPELINE (App -> Modem -> Queue)
      // =================================================================
      if (pfds[0].revents & POLLIN) {
        ssize_t bytes_read =
            read(client_fd, rx_buffer.data(), rx_buffer.size());
        if (bytes_read <= 0)
          break; // Client closed connection

        size_t offset = 0;
        while (offset < static_cast<size_t>(bytes_read)) {
          // Frame up to MAX_PAYLOAD_SIZE bytes at a time
          size_t payload_size = std::min<size_t>(bytes_read - offset, MAX_PAYLOAD_SIZE);

          Frame frame(0x01, current_seq++, payload_size, std::vector<uint8_t>(rx_buffer.begin() + offset,
                                                                              rx_buffer.begin() + offset + payload_size));

          // COBS Encode
          std::vector<uint8_t> tx_encoded = frame.cobs_encode();
          tx_encoded.push_back(0x00); // Frame delimiter

          if (tx_queue_.empty()) {
            queue_start_time_ = now;
          }
          tx_queue_.push_back(tx_encoded);
          offset += payload_size;
        }
      }

      // =================================================================
      // EVENT 2: RX PIPELINE (DSP -> Modem -> App)
      // =================================================================
      if (pfds[1].revents & POLLIN) {
        std::vector<uint8_t> pipe_buf(1024);
        ssize_t bytes = read(rx_pipe_[0], pipe_buf.data(), pipe_buf.size());

        if (bytes > 0) {
          // Append new bytes to our stream accumulator
          rx_stream_accum.insert(rx_stream_accum.end(), pipe_buf.begin(),
                                 pipe_buf.begin() + bytes);

          // Hunt for COBS 0x00 frame boundaries in the stream
          auto it =
              std::find(rx_stream_accum.begin(), rx_stream_accum.end(), 0x00);
          while (it != rx_stream_accum.end()) {
            // Extract the frame and remove it from the accumulator
            std::vector<uint8_t> rx_encoded(rx_stream_accum.begin(), it);
            rx_stream_accum.erase(rx_stream_accum.begin(), it + 1);

            // Decode and Verify
            Frame decoded_frame = cobs_decode_frame(rx_encoded);

            if (!decoded_frame.payload.empty()) {
              std::cout << "[RX] Valid Frame -> Type: 0x0"
                        << (int)decoded_frame.header.frame_type
                        << " | Seq: " << (int)decoded_frame.header.seq_num
                        << " | Len: " << decoded_frame.header.payload_len << "\n";

              // Send valid payload to the chat app!
              write(client_fd, decoded_frame.payload.data(),
                    decoded_frame.header.payload_len);
            } else {
              std::cerr << "[RX] Error: Frame failed CRC check!\n";
            }
          }
        }
      }
    }

    // =================================================================
    // QUEUE FLUSH EVALUATION
    // =================================================================
    if (!tx_queue_.empty()) {
      bool threshold_met =
          (tx_queue_.size() >= static_cast<size_t>(burst_limit_));
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - queue_start_time_)
                         .count();
      bool timeout_met = (elapsed >= flush_timeout_ms_);

      if (threshold_met || timeout_met) {
        std::cout << "[MAC] Flushing TX queue (" << tx_queue_.size()
                  << " frames). "
                  << (threshold_met ? "Threshold Met." : "Timeout Met.")
                  << "\n";

        // 1. Build Final Burst (Placeholder Preamble + All Frames)
        std::vector<uint8_t> burst_payload;

        // [Insert BPSK Training Sequence here]
        // std::vector<uint8_t> preamble = { ... };
        // burst_payload.insert(burst_payload.end(), preamble.begin(),
        // preamble.end());

        for (const auto &frame : tx_queue_) {
          burst_payload.insert(burst_payload.end(), frame.begin(), frame.end());
        }

        // 2. Hardware TX Cycle
        radio_.set_ptt(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(80)); // Relays

        dsp_.transmit_burst(burst_payload);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)); // ALSA Drain
        radio_.set_ptt(false);

        // 3. Reset
        tx_queue_.clear();
      }
    }
  }
  active_client_fd_ = -1;
}

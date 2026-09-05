#include "DataSocket.hpp"
#include "Frame.hpp"
#include "ModemDSP.hpp"
#include "RadioController.hpp"
#include "cobs.hpp"
#include <algorithm>
#include <cstring>
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
  tx_pipe_[0] = -1;
  tx_pipe_[1] = -1;
  active_client_fd_ = -1;
}

DataSocket::~DataSocket() { stop(); }

bool DataSocket::start() {
  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0)
    return false;

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
  unlink(socket_path_.c_str());

  if (bind(server_fd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    return false;
  if (listen(server_fd_, 5) < 0)
    return false;

  if (pipe(rx_pipe_) < 0)
    return false;
  if (pipe(tx_pipe_) < 0)
    return false; // Create TX Pipe

  if (!dsp_.start_rx(rx_pipe_[1]))
    return false;
  if (!dsp_.start_tx(tx_pipe_[0]))
    return false;

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
  dsp_.stop_tx();

  if (rx_pipe_[0] >= 0) {
    close(rx_pipe_[0]);
    rx_pipe_[0] = -1;
  }
  if (rx_pipe_[1] >= 0) {
    close(rx_pipe_[1]);
    rx_pipe_[1] = -1;
  }
  if (tx_pipe_[0] >= 0) {
    close(tx_pipe_[0]);
    tx_pipe_[0] = -1;
  }
  if (tx_pipe_[1] >= 0) {
    close(tx_pipe_[1]);
    tx_pipe_[1] = -1;
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
    if (poll(&pfd, 1, 100) > 0 && (pfd.revents & POLLIN)) {
      int client_fd = accept(server_fd_, nullptr, nullptr);
      if (client_fd >= 0) {
        handle_client(client_fd, stoken);
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
  std::vector<uint8_t> rx_stream_accum;
  uint8_t current_seq = 0;
  tx_queue_.clear();

  while (!stoken.stop_requested()) {
    int ret = poll(pfds, 2, 10); // 10ms loop allows fast deadline checking
    auto now = std::chrono::steady_clock::now();

    // 1. Unkey PTT if deadline has passed
    if (is_transmitting_ && now >= ptt_drop_time_) {
      radio_.set_ptt(false);
      is_transmitting_ = false;
      frames_sent_in_burst_ = 0;
      std::cout << "[MAC] Burst finished. PTT dropped.\n";
    }

    if (ret > 0) {
      // 2. Read from App Socket
      if (pfds[0].revents & POLLIN) {
        ssize_t bytes_read =
            read(client_fd, rx_buffer.data(), rx_buffer.size());
        if (bytes_read <= 0)
          break;

        size_t offset = 0;
        while (offset < static_cast<size_t>(bytes_read)) {
          size_t payload_size =
              std::min<size_t>(bytes_read - offset, MAX_PAYLOAD_SIZE);
          Frame frame(
              FrameType::DATA, current_seq++, payload_size,
              std::vector<uint8_t>(rx_buffer.begin() + offset,
                                   rx_buffer.begin() + offset + payload_size));

          std::vector<uint8_t> tx_encoded = frame.cobs_encode();
          tx_encoded.push_back(0x00);

          if (tx_queue_.empty())
            queue_start_time_ = now;
          tx_queue_.push_back(tx_encoded);
          offset += payload_size;
        }
      }

      // 3. Read from DSP RX Pipe
      if (pfds[1].revents & POLLIN) {
        std::vector<uint8_t> pipe_buf(1024);
        ssize_t bytes = read(rx_pipe_[0], pipe_buf.data(), pipe_buf.size());
        if (bytes > 0) {
          rx_stream_accum.insert(rx_stream_accum.end(), pipe_buf.begin(),
                                 pipe_buf.begin() + bytes);
          auto it =
              std::find(rx_stream_accum.begin(), rx_stream_accum.end(), 0x00);
          while (it != rx_stream_accum.end()) {
            std::vector<uint8_t> rx_encoded(rx_stream_accum.begin(), it);
            rx_stream_accum.erase(rx_stream_accum.begin(), it + 1);

            Frame decoded_frame = cobs_decode_frame(rx_encoded);
            if (!decoded_frame.payload.empty()) {
              std::cout << "[RX] Valid Frame -> Type: 0x0"
                        << static_cast<int>(decoded_frame.frame_type)
                        << " | Seq: " << (int)decoded_frame.seq_num
                        << " | Len: " << decoded_frame.payload_len << "\n";
              write(client_fd, decoded_frame.payload.data(),
                    decoded_frame.payload_len);
            } else {
              std::cerr << "[RX] Error: Frame failed CRC check!\n";
            }
            it =
                std::find(rx_stream_accum.begin(), rx_stream_accum.end(), 0x00);
          }
        }
      }
    }

    // 4. TX Queue Processing & Deadlines
    if (!tx_queue_.empty() && now >= tx_resume_time_) {
      bool threshold_met =
          (tx_queue_.size() >= static_cast<size_t>(burst_limit_));
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - queue_start_time_)
                         .count();
      bool timeout_met = (elapsed >= flush_timeout_ms_);

      // If we are already transmitting, aggressively flush the queue
      if (threshold_met || timeout_met || is_transmitting_) {

        std::vector<uint8_t> stream_chunk;

        // Key up if we aren't already
        if (!is_transmitting_) {
          radio_.set_ptt(true);
          is_transmitting_ = true;
          ptt_drop_time_ =
              now + std::chrono::milliseconds(80); // Hardware relay delay

          // Prepend 16-byte Training Preamble on first key-up
          for (int i = 0; i < 16; ++i)
            stream_chunk.push_back(0xAA);
        }

        // Pull frames up to burst limit
        while (!tx_queue_.empty() && frames_sent_in_burst_ < burst_limit_) {
          stream_chunk.insert(stream_chunk.end(), tx_queue_.front().begin(),
                              tx_queue_.front().end());
          tx_queue_.erase(tx_queue_.begin());
          frames_sent_in_burst_++;
        }

        // Stream into continuous GNU Radio pipe
        write(tx_pipe_[1], stream_chunk.data(), stream_chunk.size());

        // Add exact audio duration to our future unkey deadline (48000 Hz, 5
        // SPS, 3 bits/sym)
        double total_symbols = (stream_chunk.size() * 8.0) / 3.0;
        int duration_ms =
            static_cast<int>((total_symbols * 5.0 * 1000.0) / 48000.0);

        ptt_drop_time_ += std::chrono::milliseconds(duration_ms);

        // Enforce Half-Duplex MAC Cooldown if limit reached
        if (frames_sent_in_burst_ >= burst_limit_) {
          // Extend PTT drop slightly to allow ALSA buffer drain
          ptt_drop_time_ += std::chrono::milliseconds(100);
          tx_resume_time_ = ptt_drop_time_ + std::chrono::milliseconds(
                                                 300); // 300ms quiet turnaround
        }
      }
    }
  }
  active_client_fd_ = -1;
}

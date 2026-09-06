#include "DataSocket.hpp"
#include "Frame.hpp"
#include "ModemDSP.hpp"
#include "RadioController.hpp"
#include "cobs.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace {
constexpr int LISTEN_BACKLOG_SIZE = 5;
constexpr int RX_BUFFER_SIZE = 2048;
constexpr int PIPE_BUFFER_SIZE = 1024;
constexpr int POLL_TIMEOUT_MS = 10;
constexpr int PTT_KEY_UP_DELAY_MS = 80;
constexpr int QUIET_TURNAROUND_DELAY_MS = 300;
constexpr int TRAINING_PREAMBLE_SIZE = 16;
constexpr uint8_t TRAINING_PREAMBLE_BYTE = 0xAA;
constexpr double SYMBOLS_PER_BIT = 3.0;
constexpr double SAMPLE_RATE = 48000.0;
constexpr double SAMPLES_PER_SYMBOL = 5.0;
constexpr double SECONDS_TO_MS = 1000.0;
constexpr int DEFAULT_BURST_LIMIT = 8;
constexpr int PTT_DRAIN_DELAY_MS = 100;
constexpr double BYTES_PER_SYMBOL = 8.0;
constexpr int CYCLES_PER_SYMBOL = 3;
} // namespace

DataSocket::DataSocket(std::string socket_path, RadioController &radio,
                       ModemDSP &dsp, int burst_limit, int flush_timeout_ms)
    : socket_path_(std::move(socket_path)), server_fd_(-1), radio_(radio),
      dsp_(dsp), burst_limit_(burst_limit),
      flush_timeout_ms_(flush_timeout_ms) {
  rx_pipe_[0] = -1;
  rx_pipe_[1] = -1;
  tx_pipe_[0] = -1;
  tx_pipe_[1] = -1;
  active_client_fd_ = -1;
}

DataSocket::~DataSocket() { stop(); }

auto DataSocket::start() -> bool {
  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) [[unlikely]] {
    return false;
  }

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
  unlink(socket_path_.c_str());

  if (bind(server_fd_, reinterpret_cast<struct sockaddr *>(&addr),
           sizeof(addr)) < 0) [[unlikely]] {
    return false;
  }
  if (listen(server_fd_, LISTEN_BACKLOG_SIZE) < 0) [[unlikely]] {
    return false;
  }

  if (pipe(rx_pipe_) < 0) [[unlikely]] {
    return false;
  }
  if (pipe(tx_pipe_) < 0) [[unlikely]] {
    return false;
  }

  if (!dsp_.start_rx(rx_pipe_[1])) [[unlikely]] {
    return false;
  }
  if (!dsp_.start_tx(tx_pipe_[0])) [[unlikely]] {
    return false;
  }

  worker_thread_ = std::jthread([this](std::stop_token &&stoken) -> void {
    this->accept_loop(std::move(stoken));
  });
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
    if (poll(&pfd, 1, POLL_TIMEOUT_MS) > 0 && (pfd.revents & POLLIN)) {
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
  std::array<struct pollfd, 2> poll_fds{{}};
  poll_fds[0].fd = client_fd;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = rx_pipe_[0];
  poll_fds[1].events = POLLIN;

  std::vector<uint8_t> rx_buffer(RX_BUFFER_SIZE);
  std::vector<uint8_t> rx_stream_accum;
  uint8_t current_seq = 0;
  tx_queue_.clear();

  while (!stoken.stop_requested()) {
    int poll_result = poll(poll_fds.data(), 2, POLL_TIMEOUT_MS);
    auto now = std::chrono::steady_clock::now();

    // 1. Unkey PTT if deadline has passed
    if (is_transmitting_ && now >= ptt_drop_time_) {
      radio_.set_ptt(false);
      is_transmitting_ = false;
      frames_sent_in_burst_ = 0;
      std::cout << "[MAC] Burst finished. PTT dropped.\n";
    }

    if (poll_result > 0) {
      // 2. Read from App Socket
      if (poll_fds[0].revents & POLLIN) {
        ssize_t bytes_read =
            read(client_fd, rx_buffer.data(), rx_buffer.size());
        if (bytes_read <= 0) [[unlikely]] {
          break;
        }

        std::size_t offset = 0;
        while (static_cast<std::size_t>(bytes_read) > offset) {
          std::size_t payload_size = std::min(
              static_cast<std::size_t>(bytes_read - offset), MAX_PAYLOAD_SIZE);
          Frame frame(
              FrameType::DATA, current_seq++, payload_size,
              std::vector<uint8_t>(rx_buffer.begin() + offset,
                                   rx_buffer.begin() + offset + payload_size));

          std::vector<uint8_t> tx_encoded = frame.cobs_encode();
          tx_encoded.push_back(0x00);

          if (tx_queue_.empty()) [[unlikely]] {
            queue_start_time_ = now;
          }
          tx_queue_.push_back(std::move(tx_encoded));
          offset += payload_size;
        }
      }

      // 3. Read from DSP RX Pipe
      if (poll_fds[1].revents & POLLIN) {
        std::vector<uint8_t> pipe_buf(PIPE_BUFFER_SIZE);
        ssize_t bytes = read(rx_pipe_[0], pipe_buf.data(), pipe_buf.size());
        if (bytes > 0) {
          rx_stream_accum.insert(rx_stream_accum.end(), pipe_buf.begin(),
                                 pipe_buf.begin() +
                                     static_cast<std::ptrdiff_t>(bytes));
          auto iterator =
              std::find(rx_stream_accum.begin(), rx_stream_accum.end(), 0x00);
          while (iterator != rx_stream_accum.end()) {
            std::vector<uint8_t> rx_encoded(rx_stream_accum.begin(), iterator);
            rx_stream_accum.erase(rx_stream_accum.begin(), iterator + 1);

            Frame decoded_frame = cobs_decode_frame(rx_encoded);
            if (!decoded_frame.payload.empty()) {
              std::cout << "[RX] Valid Frame -> Type: 0x0"
                        << static_cast<int>(decoded_frame.frame_type)
                        << " | Seq: " << static_cast<int>(decoded_frame.seq_num)
                        << " | Len: " << decoded_frame.payload_len << "\n";
              write(client_fd, decoded_frame.payload.data(),
                    static_cast<std::size_t>(decoded_frame.payload_len));
            } else {
              std::cerr << "[RX] Error: Frame failed CRC check!\n";
            }
            iterator =
                std::find(rx_stream_accum.begin(), rx_stream_accum.end(), 0x00);
          }
        }
      }
    }

    // 4. TX Queue Processing & Deadlines
    if (!tx_queue_.empty() && now >= tx_resume_time_) {
      bool threshold_met =
          tx_queue_.size() >= static_cast<std::size_t>(burst_limit_);
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
          ptt_drop_time_ = now + std::chrono::milliseconds(PTT_KEY_UP_DELAY_MS);

          // Prepend 16-byte Training Preamble on first key-up
          for (int i = 0; i < TRAINING_PREAMBLE_SIZE; ++i) {
            stream_chunk.push_back(TRAINING_PREAMBLE_BYTE);
          }
        }

        // Pull frames up to burst limit
        while (!tx_queue_.empty() && frames_sent_in_burst_ < burst_limit_) {
          stream_chunk.insert(stream_chunk.end(), tx_queue_.front().begin(),
                              tx_queue_.front().end());
          tx_queue_.erase(tx_queue_.begin());
          frames_sent_in_burst_++;
        }

        // Stream into continuous GNU Radio pipe
        write(tx_pipe_[1], stream_chunk.data(),
              static_cast<int>(stream_chunk.size()));

        // Add exact audio duration to our future unkey deadline (48000 Hz, 5
        // SPS, 3 bits/sym)
        double total_symbols =
            (static_cast<double>(stream_chunk.size()) * BYTES_PER_SYMBOL) /
            CYCLES_PER_SYMBOL;
        int duration_ms = static_cast<int>(
            (total_symbols * SAMPLES_PER_SYMBOL * SECONDS_TO_MS) / SAMPLE_RATE);

        ptt_drop_time_ += std::chrono::milliseconds(duration_ms);

        // Enforce Half-Duplex MAC Cooldown if limit reached
        if (frames_sent_in_burst_ >= burst_limit_) {
          // Extend PTT drop slightly to allow ALSA buffer drain
          ptt_drop_time_ += std::chrono::milliseconds(100);
          tx_resume_time_ = ptt_drop_time_ + std::chrono::milliseconds(
                                                 QUIET_TURNAROUND_DELAY_MS);
        }
      }
    }
  }
  active_client_fd_ = -1;
}

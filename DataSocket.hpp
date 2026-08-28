#pragma once

#include <string>
#include <thread>
#include <vector>
#include <cstdint>

class DataSocket {
public:
    DataSocket(const std::string& socket_path, RadioController& radio, ModemDSP& dsp);
  ~DataSocket();

    // Starts the background thread to listen for IPC connections
    bool start();

    // Stops the background thread and cleans up the socket file
    void stop();

private:
    void accept_loop(std::stop_token stoken);
    void handle_client(int client_fd, std::stop_token& stoken);

    std::string socket_path_;
    int server_fd_;
    std::jthread worker_thread_;
    RadioController& radio_;
    ModemDSP& dsp_;
    int rx_pipe_[2]; // rx_pipe_[0] is read (C++), rx_pipe_[1] is write (GNU Radio)
    int active_client_fd_; // Track connected client to send RF data back to
};

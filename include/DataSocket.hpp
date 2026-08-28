#pragma once

#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <chrono>

// Forward declarations to fix the compiler errors
class RadioController;
class ModemDSP;

class DataSocket {
public:
    DataSocket(const std::string& socket_path, RadioController& radio, ModemDSP& dsp, 
               int burst_limit, int flush_timeout_ms);
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

    int burst_limit_;
    int flush_timeout_ms_;
    
    int rx_pipe_[2];
    int active_client_fd_;

    std::vector<std::vector<uint8_t>> tx_queue_;
    std::chrono::steady_clock::time_point queue_start_time_;
};
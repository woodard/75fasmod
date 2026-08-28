#include "DataSocket.hpp"
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>

DataSocket::DataSocket(const std::string& socket_path) 
    : socket_path_(socket_path), server_fd_(-1) {}

DataSocket::~DataSocket() {
    stop();
}

bool DataSocket::start() {
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Error: Could not create Unix socket.\n";
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    // Unlink the file if it already exists from a previous crash
    unlink(socket_path_.c_str());

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error: Could not bind Unix socket to " << socket_path_ << "\n";
        close(server_fd_);
        return false;
    }

    if (listen(server_fd_, 5) < 0) {
        std::cerr << "Error: Could not listen on Unix socket.\n";
        close(server_fd_);
        return false;
    }

    // Spawn the C++20 background thread
    worker_thread_ = std::jthread(&DataSocket::accept_loop, this);
    std::cout << "Data socket listening on " << socket_path_ << "\n";
    
    return true;
}

void DataSocket::stop() {
    if (worker_thread_.joinable()) {
        worker_thread_.request_stop(); // Signal the jthread to stop
        worker_thread_.join();         // Wait for it to finish
    }
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
        unlink(socket_path_.c_str());  // Clean up the socket file
    }
}

void DataSocket::accept_loop(std::stop_token stoken) {
    struct pollfd pfd{};
    pfd.fd = server_fd_;
    pfd.events = POLLIN;

    while (!stoken.stop_requested()) {
        // Poll with a 100ms timeout so we can frequently check the stop_token
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

void DataSocket::handle_client(int client_fd, std::stop_token& stoken) {
    struct pollfd pfd{};
    pfd.fd = client_fd;
    pfd.events = POLLIN;
    std::vector<uint8_t> buffer(2048);

    while (!stoken.stop_requested()) {
        int ret = poll(&pfd, 1, 100);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t bytes_read = read(client_fd, buffer.data(), buffer.size());
            if (bytes_read <= 0) {
                break; // Client closed connection or error
            }
            
            // TODO: Route these bytes to the COBS encoder and physical TX pipeline.
            // For now, just print what we received.
            std::cout << "[TX Pipeline Queue] Received " << bytes_read << " bytes from client.\n";
            
            // Optional: Echo back to simulate receiving data from RF (RX Pipeline)
            // std::string ack = "Simulated RF ACK\n";
            // write(client_fd, ack.data(), ack.size());
        }
    }
}
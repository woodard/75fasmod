#include "DataSocket.hpp"
#include "Frame.hpp"
#include "cobs.hpp"
#include "crc32.hpp"
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <iomanip>

DataSocket::DataSocket(const std::string& socket_path, RadioController& radio, ModemDSP& dsp) 
  : socket_path_(socket_path), server_fd_(-1), radio_(radio), dsp_(dsp) {}


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
    
    std::vector<uint8_t> rx_buffer(2048);
    uint8_t current_seq = 0;

    while (!stoken.stop_requested()) {
        int ret = poll(&pfd, 1, 100);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t bytes_read = read(client_fd, rx_buffer.data(), rx_buffer.size());
            if (bytes_read <= 0) break;
            
            // --- 1. TX PIPELINE: Frame the raw data ---
            
            // For this example, if the payload is huge, we should chunk it.
            // We'll just take up to 512 bytes for a single frame.
            size_t payload_size = std::min<size_t>(bytes_read, 512);
            
            ModemHeader header;
            header.frame_type = 0x01; // DATA frame
            header.seq_num = current_seq++;
            header.payload_len = payload_size;

            // Assemble unencoded frame: [Header] + [Payload]
            std::vector<uint8_t> raw_frame;
            uint8_t* hdr_ptr = reinterpret_cast<uint8_t*>(&header);
            raw_frame.insert(raw_frame.end(), hdr_ptr, hdr_ptr + sizeof(ModemHeader));
            raw_frame.insert(raw_frame.end(), rx_buffer.begin(), rx_buffer.begin() + payload_size);

            // Calculate and append CRC-32
            uint32_t crc = calculate_crc32(raw_frame.data(), raw_frame.size());
            uint8_t* crc_ptr = reinterpret_cast<uint8_t*>(&crc);
            raw_frame.insert(raw_frame.end(), crc_ptr, crc_ptr + sizeof(uint32_t));

            // COBS Encode and append the 0x00 delimiter
	    std::vector<uint8_t> tx_encoded = cobs_encode(raw_frame);
            tx_encoded.push_back(0x00);
            
            std::cout << "\n[MAC] Frame prepared. Size: " << tx_encoded.size() << " bytes.\n";

            // --- TRANSMIT STATE MACHINE ---
            std::cout << "[MAC] Keying PTT ON...\n";
            radio_.set_ptt(true);
            
            // Wait for RF PA and relays to settle (80ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(80));

            // Run DSP flowgraph
            dsp_.transmit_burst(tx_encoded);

            // Wait for ALSA hardware buffers to drain out the USB port (approx 50ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::cout << "[MAC] Keying PTT OFF...\n";
            radio_.set_ptt(false);
	    
            // --- 2. RX PIPELINE SIMULATION: Decode and verify ---
            
            // Remove the 0x00 delimiter before decoding
            tx_encoded.pop_back(); 
            std::vector<uint8_t> decoded_frame = cobs_decode(tx_encoded);

            if (decoded_frame.size() < sizeof(ModemHeader) + sizeof(uint32_t)) {
                std::cerr << "[RX] Error: Frame too short!\n";
                continue;
            }

            // Extract Header
            ModemHeader rx_header;
            std::memcpy(&rx_header, decoded_frame.data(), sizeof(ModemHeader));

            // Verify CRC-32
            size_t data_len_without_crc = decoded_frame.size() - sizeof(uint32_t);
            uint32_t rx_crc;
            std::memcpy(&rx_crc, decoded_frame.data() + data_len_without_crc, sizeof(uint32_t));
            uint32_t calc_crc = calculate_crc32(decoded_frame.data(), data_len_without_crc);

            std::cout << "[RX] Decoded Header -> Type: 0x0" << (int)rx_header.frame_type 
                      << " | Seq: " << (int)rx_header.seq_num 
                      << " | Payload Len: " << rx_header.payload_len << "\n";
            
            if (rx_crc == calc_crc) {
                std::cout << "[RX] CRC-32 Check: VALID (0x" << std::hex << rx_crc << std::dec << ")\n";
                // Print a preview of the payload
                std::string payload_preview(
                    reinterpret_cast<char*>(decoded_frame.data() + sizeof(ModemHeader)), 
                    std::min<size_t>(rx_header.payload_len, 20) // Print up to 20 chars
                );
                // Clean up newlines for console
                payload_preview.erase(std::remove(payload_preview.begin(), payload_preview.end(), '\n'), payload_preview.end());
                std::cout << "[RX] Payload preview: \"" << payload_preview << "\"\n";
            } else {
                std::cerr << "[RX] CRC-32 Check: FAILED!\n";
            }
        }
    }
}

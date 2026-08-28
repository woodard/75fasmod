#include "cobs.hpp"

Frame cobs_decode_frame(const std::vector<uint8_t>& input) {
  // First decode COBS
  std::vector<uint8_t> decoded;
  decoded.reserve(input.size());

  size_t read_index = 0;
  while (read_index < input.size()) {
    uint8_t code = input[read_index];
    if (read_index + code > input.size() && code != 1) {
      return Frame(); // Malformed COBS
    }
    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      decoded.push_back(input[read_index++]);
    }
    if (code < 0xFF && read_index < input.size()) {
      decoded.push_back(0);
    }
  }
  
  // Parse the decoded frame
  Frame frame;
  if (decoded.size() >= sizeof(ModemHeader) + sizeof(uint32_t)) {
    std::memcpy(&frame.header, decoded.data(), sizeof(ModemHeader));
    
    size_t data_len_without_crc = decoded.size() - sizeof(uint32_t);
    frame.payload.assign(decoded.begin() + sizeof(ModemHeader),
                         decoded.begin() + data_len_without_crc);
    
    uint32_t rx_crc;
    std::memcpy(&rx_crc, decoded.data() + data_len_without_crc, sizeof(uint32_t));
    frame.crc = rx_crc;
  }
  
  return frame;
}

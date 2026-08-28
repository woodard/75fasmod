#include "cobs.hpp"

std::vector<uint8_t> cobs_encode(const Frame& frame) {
  // Build raw frame data: header + payload + crc
  std::vector<uint8_t> raw_frame;
  const uint8_t* hdr_ptr = reinterpret_cast<const uint8_t*>(&frame.header);
  raw_frame.insert(raw_frame.end(), hdr_ptr, hdr_ptr + sizeof(ModemHeader));
  raw_frame.insert(raw_frame.end(), frame.payload.begin(), frame.payload.end());
  
  uint32_t crc = frame.calc_crc();
  uint8_t* crc_ptr = reinterpret_cast<uint8_t*>(&crc);
  raw_frame.insert(raw_frame.end(), crc_ptr, crc_ptr + sizeof(uint32_t));
  
  // COBS encode the raw frame
  std::vector<uint8_t> output;
  output.resize(raw_frame.size() + raw_frame.size() / 254 + 2);

  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < raw_frame.size()) {
    if (raw_frame[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = raw_frame[read_index++];
      code++;
      if (code == 0xFF) {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }
  output[code_index] = code;
  output.resize(write_index);
  return output;
}

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

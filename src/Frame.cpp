#include "Frame.hpp"

Frame::Frame(FrameType frame_type, uint8_t seq_num, uint16_t payload_len, std::vector<uint8_t> payload)
    : ModemHeader(frame_type, seq_num, payload_len), payload(std::move(payload)) {}

uint32_t Frame::calc_crc() const {
  uint32_t crc = 0xFFFFFFFF;
  
  // Hash header
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&header);
  size_t len = sizeof(ModemHeader);
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
    }
  }
  
  // Hash payload
  for (size_t i = 0; i < payload.size(); ++i) {
    crc ^= payload[i];
    for (int j = 0; j < 8; ++j) {
      crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
    }
  }
  
  return ~crc;
}

std::vector<uint8_t> Frame::cobs_encode() const {
  // Build raw frame data: header + payload + crc
  std::vector<uint8_t> raw_frame;
  const uint8_t* hdr_ptr = reinterpret_cast<const uint8_t*>(&header);
  raw_frame.insert(raw_frame.end(), hdr_ptr, hdr_ptr + sizeof(ModemHeader));
  raw_frame.insert(raw_frame.end(), payload.begin(), payload.end());
  
  uint32_t crc = calc_crc();
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

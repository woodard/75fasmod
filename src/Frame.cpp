#include "Frame.hpp"

Frame::Frame(ModemHeader hdr, std::vector<uint8_t> payload)
    : header(hdr), payload(std::move(payload)) {}

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

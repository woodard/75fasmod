#include "Frame.hpp"

namespace {
constexpr uint32_t CRC_INITIAL_VALUE = 0xFFFFFFFF;
constexpr uint32_t CRC_POLY = 0xEDB88320;
constexpr int CRC_BITS = 8;
constexpr uint8_t COBS_CODE_MASK = 0xFF;
constexpr int COBS_CODE_OVERHEAD = 254;
} // namespace

Frame::Frame(FrameType frame_type, uint8_t seq_num, uint16_t payload_len,
             std::vector<uint8_t> payload)
    : ModemHeader(frame_type, seq_num, payload_len),
      payload(std::move(payload)) {}

auto Frame::calc_crc() const -> uint32_t {
  uint32_t crc = CRC_INITIAL_VALUE;

  // Hash inherited header fields
  auto data =
      reinterpret_cast<const uint8_t *>(static_cast<const ModemHeader *>(this));
  size_t len = sizeof(ModemHeader);
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < CRC_BITS; ++j) {
      crc = (crc >> 1) ^ (CRC_POLY & (-(crc & 1)));
    }
  }

  // Hash payload using range-based for loop
  for (auto &byte : payload) {
    crc ^= byte;
    for (int j = 0; j < CRC_BITS; ++j) {
      crc = (crc >> 1) ^ (CRC_POLY & (-(crc & 1)));
    }
  }

  return ~crc;
}

auto Frame::cobs_encode() const -> std::vector<uint8_t> {
  std::vector<uint8_t> raw_frame;
  auto hdr_ptr =
      reinterpret_cast<const uint8_t *>(static_cast<const ModemHeader *>(this));
  raw_frame.insert(raw_frame.end(), hdr_ptr, hdr_ptr + sizeof(ModemHeader));
  raw_frame.insert(raw_frame.end(), payload.begin(), payload.end());

  uint32_t crc = calc_crc();
  uint8_t *crc_ptr = reinterpret_cast<uint8_t *>(&crc);
  raw_frame.insert(raw_frame.end(), crc_ptr, crc_ptr + sizeof(uint32_t));

  std::vector<uint8_t> output;
  output.resize(raw_frame.size() + (raw_frame.size() / COBS_CODE_OVERHEAD) + 2);

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
      if (code == COBS_CODE_MASK) {
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

#include "Frame.hpp"

/**
 * @file Frame.hpp
 * @brief Frame structure for 75fasmod data transmission
 *
 * This module defines the Frame class and related structures used for
 * data transmission in the 75fasmod modem. Frames are the basic
 * communication units that are encoded with COBS and transmitted over
 * the radio link.
 *
 * Frame structure:
 * - ModemHeader (frame_type, sequence_number, payload_length)
 * - Payload (variable length data)
 * - CRC32 (error detection checksum)
 */

/**
 * @brief Enumeration of frame types
 *
 * Defines the type of data contained in a frame.
 */

/**
 * @brief Modem header structure
 *
 * Contains metadata for frame identification and processing.
 */

/**
 * @brief Constructs a ModemHeader
 * @param frame_type Type of frame (DATA or ACK)
 * @param seq_num Sequence number (0-255, rolling)
 * @param payload_len Length of the payload in bytes
 */

/**
 * @brief Data frame structure
 *
 * Represents a complete data frame with header, payload, and CRC checksum.
 * Inherits from ModemHeader to include the header fields.
 */

/**
 * @brief Constructs a Frame
 * @param frame_type Type of frame (DATA or ACK)
 * @param seq_num Sequence number (0-255, rolling)
 * @param payload_len Length of the payload in bytes
 * @param payload Data payload vector
 */

/**
 * @brief Calculates CRC32 checksum
 * @return CRC32 value for the frame
 */

/**
 * @brief Encodes the frame using COBS
 * @return COBS-encoded byte vector
 */

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
  const auto *data =
      reinterpret_cast<const uint8_t *>(static_cast<const ModemHeader *>(this));
  size_t len = sizeof(ModemHeader);
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < CRC_BITS; ++j) {
      crc = (crc >> 1) ^ (CRC_POLY & (-(crc & 1)));
    }
  }

  // Hash payload using range-based for loop
  for (auto &byte : payload) { // NOLINT(readability-qualified-auto)
    crc ^= byte;
    for (int j = 0; j < CRC_BITS; ++j) {
      crc = (crc >> 1) ^ (CRC_POLY & (-(crc & 1)));
    }
  }

  return ~crc;
}

auto Frame::cobs_encode() const -> std::vector<uint8_t> {
  std::vector<uint8_t> raw_frame;
  const auto *hdr_ptr =
      reinterpret_cast<const uint8_t *>(static_cast<const ModemHeader *>(this));
  raw_frame.insert(raw_frame.end(), hdr_ptr, hdr_ptr + sizeof(ModemHeader));
  raw_frame.insert(raw_frame.end(), payload.begin(), payload.end());

  uint32_t crc = calc_crc();
  auto *crc_ptr = reinterpret_cast<uint8_t *>(&crc);
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

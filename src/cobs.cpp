#include "cobs.hpp"

/**
 * @file cobs.hpp
 * @brief COBS (Consistent Overhead Byte Stuffing) encoding and decoding
 *
 * This module provides functions to encode and decode data using the COBS
 * (Consistent Overhead Byte Stuffing) algorithm. COBS is commonly used in
 * serial communications and packet framing to ensure reliable data transfer
 * by eliminating null bytes from the data stream.
 *
 * @see Frame.hpp for the Frame class used with COBS
 */

/**
 * @brief Encodes a Frame using COBS algorithm
 *
 * Encodes a Frame object and returns the COBS-encoded bytes.
 * The trailing 0x00 byte is NOT appended to the output.
 *
 * @param frame The Frame object to encode
 * @return std::vector<uint8_t> COBS-encoded byte vector
 *
 * @note The encoded output can be transmitted over channels that cannot
 *       handle null bytes (0x00)
 *
 * @see cobs_decode_frame() for decoding
 */

/**
 * @brief Decodes COBS data and returns a Frame
 *
 * Decodes COBS-encoded data and reconstructs the original Frame object.
 *
 * @param input COBS-encoded byte vector
 * @return Frame Decoded Frame object
 *
 * @see cobs_encode() for encoding
 */

#include <cstring>

namespace {
constexpr uint8_t COBS_CODE_MASK = 0xFF;
} // namespace

auto cobs_decode_frame(const std::vector<uint8_t> &input) -> Frame {
  std::vector<uint8_t> decoded;
  decoded.reserve(input.size());

  size_t read_index = 0;
  while (read_index < input.size()) {
    uint8_t code = input[read_index];
    if (read_index + code > input.size() && code != 1) {
      return Frame{FrameType::DATA, 0, 0, {}};
    }
    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      decoded.push_back(input[read_index++]);
    }
    if (code < COBS_CODE_MASK && read_index < input.size()) {
      decoded.push_back(0);
    }
  }

  if (decoded.size() >= sizeof(ModemHeader) + sizeof(uint32_t)) {
    auto frame_type = static_cast<FrameType>(decoded[0]);
    uint8_t seq_num = decoded[1];
    uint16_t payload_len =
        *reinterpret_cast<const uint16_t *>(decoded.data() + 2);

    std::vector<uint8_t> payload(
        decoded.begin() + sizeof(ModemHeader),
        decoded.begin() + static_cast<std::ptrdiff_t>(decoded.size()) -
            sizeof(uint32_t));

    Frame frame{frame_type, seq_num, payload_len, std::move(payload)};

    uint32_t rx_crc;
    std::memcpy(&rx_crc, decoded.data() + decoded.size() - sizeof(uint32_t),
                sizeof(uint32_t));
    frame.crc = rx_crc;

    return frame;
  }

  return Frame{FrameType::DATA, 0, 0, {}};
}
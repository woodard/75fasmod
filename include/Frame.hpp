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

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#pragma pack(push, 1)

/**
 * @brief Enumeration of frame types
 *
 * Defines the type of data contained in a frame.
 */
enum class FrameType {
  DATA = 0x01, ///< Data frame containing payload
  ACK = 0x02   ///< Acknowledgment frame
};

/**
 * @brief Modem header structure
 *
 * Contains metadata for frame identification and processing.
 */
class ModemHeader {
public:
  /**
   * @brief Constructs a ModemHeader
   * @param frame_type Type of frame (DATA or ACK)
   * @param seq_num Sequence number (0-255, rolling)
   * @param payload_len Length of the payload in bytes
   */
  ModemHeader(FrameType frame_type, uint8_t seq_num, uint16_t payload_len)
      : frame_type(frame_type), seq_num(seq_num), payload_len(payload_len) {}

  FrameType frame_type; // 0x01 = DATA, 0x02 = ACK
  uint8_t seq_num;      // 0-255 rolling sequence
  uint16_t payload_len; // Length of the inner data payload
};

#pragma pack(pop)

/// Maximum payload size per frame (128 bytes)
constexpr size_t MAX_PAYLOAD_SIZE = 128;

/**
 * @brief Data frame structure
 *
 * Represents a complete data frame with header, payload, and CRC checksum.
 * Inherits from ModemHeader to include the header fields.
 */
class Frame : public ModemHeader {
public:
  /**
   * @brief Constructs a Frame
   * @param frame_type Type of frame (DATA or ACK)
   * @param seq_num Sequence number (0-255, rolling)
   * @param payload_len Length of the payload in bytes
   * @param payload Data payload vector
   */
  Frame(FrameType frame_type, uint8_t seq_num, uint16_t payload_len,
        std::vector<uint8_t> payload);

  /**
   * @brief Calculates CRC32 checksum
   * @return CRC32 value for the frame
   */
  uint32_t calc_crc() const;

  /**
   * @brief Encodes the frame using COBS
   * @return COBS-encoded byte vector
   */
  std::vector<uint8_t> cobs_encode() const;

  std::vector<uint8_t> payload; ///< Data payload
  uint32_t crc = 0;             ///< CRC32 checksum
};

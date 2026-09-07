#pragma once // NOLINT(llvm-header-guard,cppcoreguidelines-header-guard)
#include <cstddef>
#include <cstdint>
#include <vector>

/// Type alias for sequence number to distinguish from other integer parameters
using sequence_number_t = uint8_t;
/// Type alias for payload length to distinguish from other integer parameters
using payload_length_t = uint16_t;

#pragma pack(push, 1)

enum class FrameType : std::uint8_t {
  DATA = 0x01, ///< Data frame containing payload
  ACK = 0x02   ///< Acknowledgment frame
};

class ModemHeader {
public:
  ModemHeader(
      FrameType frame_type,
      sequence_number_t seq_num, // NOLINT(bugprone-easily-swappable-parameters)
      payload_length_t payload_len)
      : frame_type(frame_type), seq_num(seq_num), payload_len(payload_len) {}

  FrameType frame_type; // 0x01 = DATA, 0x02 = ACK
  uint8_t seq_num;      // 0-255 rolling sequence
  uint16_t payload_len; // Length of the inner data payload
};

#pragma pack(pop)

/// Maximum payload size per frame (128 bytes)
constexpr size_t MAX_PAYLOAD_SIZE = 128;

class Frame : public ModemHeader {
public:
  Frame(FrameType frame_type, uint8_t seq_num, uint16_t payload_len,
        std::vector<uint8_t> payload);

  [[nodiscard]] auto calc_crc() const -> uint32_t;

  [[nodiscard]] auto cobs_encode() const -> std::vector<uint8_t>;

  std::vector<uint8_t> payload; ///< Data payload
  uint32_t crc = 0;             ///< CRC32 checksum
};

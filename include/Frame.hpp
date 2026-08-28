#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#pragma pack(push, 1)
class ModemHeader {
public:
  ModemHeader(uint8_t frame_type, uint8_t seq_num, uint16_t payload_len)
      : frame_type(frame_type), seq_num(seq_num), payload_len(payload_len) {}

  uint8_t frame_type;   // 0x01 = DATA, 0x02 = ACK
  uint8_t seq_num;      // 0-255 rolling sequence
  uint16_t payload_len; // Length of the inner data payload
};
#pragma pack(pop)

// Maximum payload size per frame
constexpr size_t MAX_PAYLOAD_SIZE = 128;

class Frame : public ModemHeader {
public:
  Frame(uint8_t frame_type, uint8_t seq_num, uint16_t payload_len, std::vector<uint8_t> payload)
      : ModemHeader(frame_type, seq_num, payload_len), payload(std::move(payload)) {}

  // Calculate CRC-32 for this frame's header + payload
  uint32_t calc_crc() const;
  
  // COBS encode the frame
  std::vector<uint8_t> cobs_encode() const;
  
  std::vector<uint8_t> payload;
  uint32_t crc = 0;
};

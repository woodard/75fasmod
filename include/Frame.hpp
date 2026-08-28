#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#pragma pack(push, 1)
struct ModemHeader {
  uint8_t frame_type;   // 0x01 = DATA, 0x02 = ACK
  uint8_t seq_num;      // 0-255 rolling sequence
  uint16_t payload_len; // Length of the inner data payload
};
#pragma pack(pop)

// Maximum payload size per frame
constexpr size_t MAX_PAYLOAD_SIZE = 128;

class Frame {
public:
  Frame() = default;
  Frame(ModemHeader hdr, std::vector<uint8_t> payload);
  
  // Calculate CRC-32 for this frame's header + payload
  uint32_t calc_crc() const;
  
  // COBS encode the frame
  std::vector<uint8_t> cobs_encode() const;
  
  ModemHeader header;
  std::vector<uint8_t> payload;
  uint32_t crc = 0;
};

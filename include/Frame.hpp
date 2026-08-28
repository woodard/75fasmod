#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct ModemHeader {
  uint8_t frame_type;   // 0x01 = DATA, 0x02 = ACK
  uint8_t seq_num;      // 0-255 rolling sequence
  uint16_t payload_len; // Length of the inner data payload
};
#pragma pack(pop)

// Maximum payload size per frame
constexpr size_t MAX_PAYLOAD_SIZE = 128;

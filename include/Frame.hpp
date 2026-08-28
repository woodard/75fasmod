#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#pragma pack(push, 1)
enum class FrameType {
  DATA = 0x01,
  ACK = 0x02
};

class ModemHeader {
public:
  ModemHeader(FrameType frame_type, uint8_t seq_num, uint16_t payload_len)
      : frame_type(frame_type), seq_num(seq_num), payload_len(payload_len) {}

  FrameType frame_type;   // 0x01 = DATA, 0x02 = ACK
  uint8_t seq_num;        // 0-255 rolling sequence
  uint16_t payload_len;   // Length of the inner data payload
};
#pragma pack(pop)

constexpr size_t MAX_PAYLOAD_SIZE = 128;

class Frame : public ModemHeader {
public:
  Frame(FrameType frame_type, uint8_t seq_num, uint16_t payload_len, std::vector<uint8_t> payload);

  uint32_t calc_crc() const;
  std::vector<uint8_t> cobs_encode() const;
  
  std::vector<uint8_t> payload;
  uint32_t crc = 0;
};
#include "cobs.hpp"
#include <cstring>

Frame cobs_decode_frame(const std::vector<uint8_t> &input) {
  std::vector<uint8_t> decoded;
  decoded.reserve(input.size());

  size_t read_index = 0;
  while (read_index < input.size()) {
    uint8_t code = input[read_index];
    if (read_index + code > input.size() && code != 1) {
      return Frame(FrameType::DATA, 0, 0, {});
    }
    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      decoded.push_back(input[read_index++]);
    }
    if (code < 0xFF && read_index < input.size()) {
      decoded.push_back(0);
    }
  }

  if (decoded.size() >= sizeof(ModemHeader) + sizeof(uint32_t)) {
    FrameType frame_type = static_cast<FrameType>(decoded[0]);
    uint8_t seq_num = decoded[1];
    uint16_t payload_len =
        *reinterpret_cast<const uint16_t *>(decoded.data() + 2);

    std::vector<uint8_t> payload(decoded.begin() + sizeof(ModemHeader),
                                 decoded.begin() + decoded.size() -
                                     sizeof(uint32_t));

    Frame frame(frame_type, seq_num, payload_len, std::move(payload));

    uint32_t rx_crc;
    std::memcpy(&rx_crc, decoded.data() + decoded.size() - sizeof(uint32_t),
                sizeof(uint32_t));
    frame.crc = rx_crc;

    return frame;
  }

  return Frame(FrameType::DATA, 0, 0, {});
}
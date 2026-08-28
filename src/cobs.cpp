#include "cobs.hpp"

std::vector<uint8_t> cobs_encode(const std::vector<uint8_t> &input) {
  std::vector<uint8_t> output;
  output.resize(input.size() + input.size() / 254 + 2);

  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < input.size()) {
    if (input[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF) {
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

std::vector<uint8_t> cobs_decode(const std::vector<uint8_t> &input) {
  std::vector<uint8_t> output;
  output.reserve(input.size()); // Max possible size

  size_t read_index = 0;
  while (read_index < input.size()) {
    uint8_t code = input[read_index];
    if (read_index + code > input.size() && code != 1) {
      return {}; // Malformed COBS
    }
    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      output.push_back(input[read_index++]);
    }
    if (code < 0xFF && read_index < input.size()) {
      output.push_back(0);
    }
  }
  return output;
}
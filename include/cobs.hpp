#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Encodes data and returns a new buffer. (Does NOT append the trailing 0x00).
std::vector<uint8_t> cobs_encode(const std::vector<uint8_t> &input);

// Decodes data. Assumes the trailing 0x00 has already been removed.
std::vector<uint8_t> cobs_decode(const std::vector<uint8_t> &input);
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Frame.hpp"

// Encodes a Frame and returns COBS-encoded bytes (does NOT append trailing 0x00)
std::vector<uint8_t> cobs_encode(const Frame& frame);

// Decodes COBS data and returns a Frame
Frame cobs_decode_frame(const std::vector<uint8_t>& input);

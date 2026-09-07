#pragma once // NOLINT(llvm-header-guard,cppcoreguidelines-header-guard)
#include "Frame.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

auto cobs_encode(const Frame &frame) -> std::vector<uint8_t>;

auto cobs_decode_frame(const std::vector<uint8_t> &input) -> Frame;

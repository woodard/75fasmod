#include "TcmConfig.hpp"
#include <cmath>
#include <stdexcept>

namespace {
constexpr int FSM_MEMORY_STATES = 8;
constexpr int FSM_INFO_BITS_DIVISOR = 2;
constexpr int CONSTELLATION_ROT_SYMMETRY = 4;
constexpr int CONSTELLATION_REAL_SECTORS = 2;
constexpr int CONSTELLATION_IMAG_SECTORS = 2;
} // namespace

auto TcmConfig::get_fsm(ModulationScheme scheme) -> gr::trellis::fsm {
  int info_bits = 3; // Default QAM16 (3 info bits -> 4 coded bits)

  switch (scheme) {
  case ModulationScheme::QAM16:
    info_bits = 3;
    break;
  case ModulationScheme::QAM32:
    info_bits = 4;
    break;
  case ModulationScheme::QAM64:
    info_bits = 5;
    break;
  case ModulationScheme::QAM128:
    info_bits = 6;
    break;
  case ModulationScheme::QAM256:
    info_bits = 7;
    break;
  }

  int num_input_symbols = 1 << info_bits; // 2^k input symbols
  int num_states = FSM_MEMORY_STATES;     // 8 internal memory states
  int num_output_symbols =
      1 << (info_bits +
            FSM_INFO_BITS_DIVISOR); // 2^(k+1) output constellation indices

  int const I = num_input_symbols; // Number of input symbols
  int const S = num_states;        // Number of states

  std::vector<int> NS(num_input_symbols * num_states);
  std::vector<int> OS(num_input_symbols * num_states);

  for (auto state = 0; state < S; ++state) {
    for (auto input_sym = 0; input_sym < I; ++input_sym) {
      int uncoded_bits = input_sym >> 1;
      int coded_bit = input_sym & 1;

      // Rate-1/2 8-state systematic feedback polynomial transitions
      int next_s = ((state << 1) | coded_bit) & (FSM_MEMORY_STATES - 1);
      int parity = ((state >> 2) ^ (state >> 1) ^ coded_bit) & 1;
      int output_symbol = (uncoded_bits << 2) | (parity << 1) | coded_bit;

      NS[state * I + input_sym] = next_s;
      OS[state * I + input_sym] = output_symbol;
    }
  }

  return gr::trellis::fsm(num_input_symbols, num_states, num_output_symbols, NS,
                          OS);
}

auto TcmConfig::get_constellation(ModulationScheme scheme)
    -> gr::digital::constellation_sptr {
  std::vector<gr_complex> points;

  switch (scheme) {
  case ModulationScheme::QAM16: {
    // 4x4 Square Grid
    for (int x = -3; x <= 3; x += 2) {
      for (int y = -3; y <= 3; y += 2) {
        points.push_back(gr_complex(x / 3.0, y / 3.0));
      }
    }
    break;
  }

  case ModulationScheme::QAM32: {
    // 6x6 Cross Grid (36 points - 4 corners = 32 points)
    for (int x = -5; x <= 5; x += 2) {
      for (int y = -5; y <= 5; y += 2) {
        if (std::abs(x) == 5 && std::abs(y) == 5)
          continue; // Skip 4 corners
        points.push_back(gr_complex(x / 5.0, y / 5.0));
      }
    }
    break;
  }

  case ModulationScheme::QAM64: {
    // 8x8 Square Grid
    for (int x = -7; x <= 7; x += 2) {
      for (int y = -7; y <= 7; y += 2) {
        points.push_back(gr_complex(x / 7.0, y / 7.0));
      }
    }
    break;
  }

  case ModulationScheme::QAM128: {
    // 12x12 Cross Grid (144 points - 16 corner points = 128 points)
    for (int x = -11; x <= 11; x += 2) {
      for (int y = -11; y <= 11; y += 2) {
        if (std::abs(x) >= 9 && std::abs(y) >= 9)
          continue; // Skip 4x4 corners
        points.push_back(gr_complex(x / 11.0, y / 11.0));
      }
    }
    break;
  }

  case ModulationScheme::QAM256: {
    // 16x16 Square Grid
    for (int x = -15; x <= 15; x += 2) {
      for (int y = -15; y <= 15; y += 2) {
        points.push_back(gr_complex(x / 15.0, y / 15.0));
      }
    }
    break;
  }
  }

  std::vector<int> symbol_map(points.size());
  for (size_t i = 0; i < points.size(); ++i) {
    symbol_map[i] = static_cast<int>(i);
  }

  return gr::digital::constellation_rect::make(
      points,     // Constellation points
      symbol_map, // Symbol map (pre_diff_code)
      4,          // Rotational symmetry (4-fold for QAM)
      2,          // Real sectors
      2,          // Imaginary sectors
      1.0,        // Width real sectors (uppercase)
      1.0,        // Width imaginary sectors (uppercase)
      gr::digital::constellation::NO_NORMALIZATION // Normalization strategy
  );
}
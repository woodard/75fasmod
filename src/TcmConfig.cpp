#include "TcmConfig.hpp"
#include <cmath>
#include <stdexcept>

gr::trellis::fsm TcmConfig::get_fsm(ModulationScheme scheme) {
  int k = 3; // Default QAM16 (3 info bits -> 4 coded bits)

  switch (scheme) {
    case ModulationScheme::QAM16:  k = 3; break;
    case ModulationScheme::QAM32:  k = 4; break;
    case ModulationScheme::QAM64:  k = 5; break;
    case ModulationScheme::QAM128: k = 6; break;
    case ModulationScheme::QAM256: k = 7; break;
  }

  int I = 1 << k;         // 2^k input symbols
  int S = 8;              // 8 internal memory states
  int O = 1 << (k + 1);   // 2^(k+1) output constellation indices

  std::vector<int> NS(I * S);
  std::vector<int> OS(I * S);

  for (int s = 0; s < S; ++s) {
    for (int i = 0; i < I; ++i) {
      int uncoded_bits = i >> 1;
      int coded_bit = i & 1;

      // Rate-1/2 8-state systematic feedback polynomial transitions
      int next_s = ((s << 1) | coded_bit) & 7;
      int parity = ((s >> 2) ^ (s >> 1) ^ coded_bit) & 1;
      int output_symbol = (uncoded_bits << 2) | (parity << 1) | coded_bit;

      NS[s * I + i] = next_s;
      OS[s * I + i] = output_symbol;
    }
  }

  return gr::trellis::fsm(I, S, O, NS, OS);
}

gr::digital::constellation_sptr TcmConfig::get_constellation(ModulationScheme scheme) {
  std::vector<gr_complex> points;

  switch (scheme) {
    case ModulationScheme::QAM16: {
      // 4x4 Square Grid
      for (int x = -3; x <= 3; x += 2) {
        for (int y = -3; y <= 3; y += 2) {
          points.push_back(gr_complex(x / 3.0f, y / 3.0f));
        }
      }
      break;
    }

    case ModulationScheme::QAM32: {
      // 6x6 Cross Grid (36 points - 4 corners = 32 points)
      for (int x = -5; x <= 5; x += 2) {
        for (int y = -5; y <= 5; y += 2) {
          if (std::abs(x) == 5 && std::abs(y) == 5) continue; // Skip 4 corners
          points.push_back(gr_complex(x / 5.0f, y / 5.0f));
        }
      }
      break;
    }

    case ModulationScheme::QAM64: {
      // 8x8 Square Grid
      for (int x = -7; x <= 7; x += 2) {
        for (int y = -7; y <= 7; y += 2) {
          points.push_back(gr_complex(x / 7.0f, y / 7.0f));
        }
      }
      break;
    }

    case ModulationScheme::QAM128: {
      // 12x12 Cross Grid (144 points - 16 corner points = 128 points)
      for (int x = -11; x <= 11; x += 2) {
        for (int y = -11; y <= 11; y += 2) {
          if (std::abs(x) >= 9 && std::abs(y) >= 9) continue; // Skip 4x4 corners
          points.push_back(gr_complex(x / 11.0f, y / 11.0f));
        }
      }
      break;
    }

    case ModulationScheme::QAM256: {
      // 16x16 Square Grid
      for (int x = -15; x <= 15; x += 2) {
        for (int y = -15; y <= 15; y += 2) {
          points.push_back(gr_complex(x / 15.0f, y / 15.0f));
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
      points,                                       // Constellation points
      symbol_map,                                   // Symbol map (pre_diff_code)
      4,                                            // Rotational symmetry (4-fold for QAM)
      2,                                            // Real sectors
      2,                                            // Imaginary sectors
      1.0f,                                         // Width real sectors
      1.0f,                                         // Width imaginary sectors
      gr::digital::constellation::NO_NORMALIZATION  // Normalization strategy
  );
}
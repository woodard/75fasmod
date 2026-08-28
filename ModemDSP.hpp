#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include <gnuradio/top_block.h>

class ModemDSP {
public:
    ModemDSP();
    ~ModemDSP() = default;

    // Takes framed data, runs it through the GR modulator, and pushes to ALSA
    void transmit_burst(const std::vector<uint8_t>& framed_data);
};
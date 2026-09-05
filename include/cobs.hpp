/**
 * @file cobs.hpp
 * @brief COBS (Consistent Overhead Byte Stuffing) encoding and decoding
 *
 * This module provides functions to encode and decode data using the COBS
 * (Consistent Overhead Byte Stuffing) algorithm. COBS is commonly used in
 * serial communications and packet framing to ensure reliable data transfer
 * by eliminating null bytes from the data stream.
 *
 * @see Frame.hpp for the Frame class used with COBS
 */

#pragma once
#include "Frame.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @brief Encodes a Frame using COBS algorithm
 *
 * Encodes a Frame object and returns the COBS-encoded bytes.
 * The trailing 0x00 byte is NOT appended to the output.
 *
 * @param frame The Frame object to encode
 * @return std::vector<uint8_t> COBS-encoded byte vector
 *
 * @note The encoded output can be transmitted over channels that cannot
 *       handle null bytes (0x00)
 *
 * @see cobs_decode_frame() for decoding
 */
std::vector<uint8_t> cobs_encode(const Frame &frame);

/**
 * @brief Decodes COBS data and returns a Frame
 *
 * Decodes COBS-encoded data and reconstructs the original Frame object.
 *
 * @param input COBS-encoded byte vector
 * @return Frame Decoded Frame object
 *
 * @see cobs_encode() for encoding
 */
Frame cobs_decode_frame(const std::vector<uint8_t> &input);

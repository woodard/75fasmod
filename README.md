# README.md

## Project Overview
This project aims to develop the fastest possible modem for the
**Kenwood TH-D75**, providing an open-source alternative to
proprietary high-speed modems like VARA FM.

While VARA FM utilizes OFDM (Orthogonal Frequency Division
Multiplexing) when modulating on FM channels, this project takes a
different approach. Inspired by the high-speed V.34 modems of the
1990s, this implementation uses **Adaptive QAM (Quadrature Amplitude
Modulation) combined with TCM (Trellis Coded Modulation)**.

The goal is to achieve performance comparable to industry-standard
proprietary software while maintaining a completely open-source
architecture.

## Technical Stack
The project is built upon two primary pillars of the amateur radio and
signal processing community:
* **GNU Radio**: Utilized for the Digital Signal Processing (DSP)
  pipeline, including modulation, filtering, and constellation
  mapping.
* **Hamlib**: Used for robust radio control and interfacing with the
  hardware.
  **ALSA** (Advanced Linux Sound Architecture): Used to route the
  processed audio signal to the radio hardware via an ALSA device

## Hardware Focus
The primary target for this project is the **Kenwood TH-D75**. This
hardware was chosen due to its exceptionally flat audio response,
which is critical for high-order modulation schemes.

*Note: The project is designed to eventually support other radios, but
the current development is optimized for the TH-D75 (identifiable via
USB ID 2166:9023 [1]).*

## Current Status: **Early Development**
**⚠️ WARNING: This project is in a highly experimental, early stage of
development. It is by no means a finished product.**

* **Modulation Pipeline**: Current work is focused on resolving
  fundamental "kinks" within the modulation side of the DSP pipeline.
* **Demodulation**: The demodulator is currently a placeholder/dummy
  implementation and does not yet perform true signal decoding.
* **Stability**: The codebase is unstable and intended for research
  and development purposes only.

## Development Methodology
This project is an active experiment in **AI-assisted software
development**. The codebase is largely authored using AI tools. A
primary motivation for this project is to explore, learn, and refine
the process of using modern AI capabilities to accelerate complex
engineering tasks and experimental software creation.

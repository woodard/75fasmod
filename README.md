# 75fasmod

[![Status:
Experimental](https://img.shields.io/badge/status-experimental-red.svg)](#)
[![License: Open
Source](https://img.shields.io/badge/license-open--source-blue.svg)](#)

**75fasmod** is an experimental, high-speed, open-source modem
designed specifically for the **Kenwood TH-D75**. The project's
mission is to achieve data rates comparable to proprietary solutions
like VARA FM by utilizing advanced modulation techniques within an
entirely open-source framework.

## 🚀 The Vision
While existing high-speed FM modems often rely on OFDM, **75fasmod**
takes inspiration from the legendary V.34 modems of the 1990s. By
utilizing **Adaptive QAM (Quadrature Amplitude Modulation)** combined
with **TCM (Trellis Coded Modulation)**, this project aims to maximize
throughput on the exceptionally flat audio response provided by the
Kenwood TH-D75 hardware.

## ✨ Key Features
* **Advanced Modulation**: Implementation of Adaptive QAM + TCM for
  high-efficiency signal transmission.
* **Robust Framing**: Utilizes **COBS** (Consistent Overhead Byte
  Stuffating) [1] and **CRC32** checksums for reliable data framing
  and error detection.
* **Hardware Optimized**: Specifically tuned for the audio
  characteristics of the Kenwood TH-D75.
* **Modern C++**: Built using **C++20** standards for high-performance
  digital signal processing [1].

## 🛠 Technical Stack
The project leverages industry-standard libraries for signal
processing and hardware interfacing:
* **GNU Radio**: The core engine for the Digital Signal Processing
  (DSP) pipeline, handling modulation, filtering, and constellation
  mapping.
* **Hamlib**: Provides the interface for robust radio control and
  command execution.
* **ALSA (Advanced Linux Sound Architecture)**: Manages the routing of
  processed digital audio to the radio hardware.

## 🏗 Architecture
The project follows a modular C++ structure:
* `include/`: Contains core logic headers including `ModemDSP.hpp`,
  `RadioController.hpp`, and `TcmConfig.hpp`.
* `src/`: Contains implementation files for the DSP pipeline and radio
  control logic.
* `test/`: Includes Python-based testing utilities (e.g.,
  `test_tx.py`) for verifying transmission sequences.

## ⚠️ Current Development Status
**This project is in a highly experimental, early stage of
development.**
* **Modulation**: The modulation pipeline is currently undergoing
  fundamental refinement to resolve DSP "kinks."
* **Demodulation**: The demodulator is currently a **dummy
  implementation** and does not yet perform full signal decoding.
* **Stability**: The codebase is unstable and intended for research
  and development purposes only.

## 🛠 Installation & Building
The project uses the **Autotools** build system.

### Prerequisites
Ensure you have the following installed:
* GNU Radio
* Hamlib
* ALSA development headers
* Autotools (`autoconf`, `automake`, `make`)
* A C++20 compatible compiler (e.g., GCC 10+)

### Build Steps
```bash
# Generate build files
./autogen.sh

# Configure the project
./configure

# Compile
make

# Format code (optional)
make format
```

## Contributing

This project is in its early, experimental stages, and we welcome all
forms of contribution!

## 🤖 Development Methodology: AI-Assisted Engineering
This project serves as a experiment in **AI-assisted software
development**. The codebase is largely authored using modern AI
tools. One of the goals is to explore how LLMs and AI agents can
accelerate complex engineering tasks. I'm being asked to use these
tools for work where the results probably actually matter. To learn, I
needed something that didn't matter to play with.

The main tools that I have used were Gemini's web interface, aider
connected to a local ollama instance running various models.

### 🛠 Bug Reports & Stability
The codebase is currently unstable and intended for research and
development . If you encounter crashes, unexpected behavior, or
audio artifacts, please open an issue.

### 📻 Hardware Testing
If you have a Kenwood TH-D75, please help us test the modulation side
of the pipeline  and report on the results. Your feedback on audio
quality is invaluable.

### 🧠 Technical Feedback
We are looking for expertise in GNU Radio, ALSA, and digital
modulation (QAM/TCM) to help resolve current development "kinks"
. If you have insights into the DSP pipeline, please reach out.

### 🤖 AI-Assisted Development
This project is an experiment in using AI to accelerate complex
engineering tasks . We welcome anyone interested in sharing
workflows, prompt engineering strategies, or tools used to assist in
the creation and refinement of this codebase.

## 👤 Author
Ben Woodard **AE6BC**

## 📄 License
GPL-3.0



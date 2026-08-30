# Technical Notes

## The Shannon limit

To determine the Shannon limit (maximum theoretical channel capacity)
for a channel with a bandwidth of **3200 Hz**, you must apply the
**Shannon-Hartley Theorem**:

$$C = B \times \log_2(1 + \text{SNR})$$

Where:

* $C$ = Channel Capacity in **bits per second (bps)**
* $B$ = Channel Bandwidth in **Hz** ($3,200 \text{ Hz}$)
* $\text{SNR}$ = Signal-to-Noise Ratio as a **linear ratio** (not in dB)

Because the Shannon limit depends heavily on the channel's
Signal-to-Noise Ratio (SNR), the maximum capacity varies based on link
quality:

### Channel Capacity at Common SNR Levels

To calculate the linear SNR from a decibel value
($\text{SNR}_{\text{dB}}$):

$$\text{SNR}_{\text{linear}} = 10^{\left(\frac{\text{SNR}_{\text{dB}}}{10}\right)}$$

| Channel Condition | $\text{SNR}_{\text{dB}}$ | $\text{SNR}_{\text{linear}}$ | $\log_2(1 + \text{SNR})$ | Maximum Capacity ($C$) |
| --- | --- | --- | --- | --- |
| **Very Weak Signal** | $0 \text{ dB}$ | $1.0$ | $\approx 1 \text{ bit/symbol}$ | **3,200 bps** (~3.2 kbps) |
| **Fair / Low Signal** | $10 \text{ dB}$ | $10.0$ | $\approx 3.46 \text{ bits/symbol}$ | **11,068 bps** (~11.1 kbps) |
| **Good Signal** | $20 \text{ dB}$ | $100.0$ | $\approx 6.66 \text{ bits/symbol}$ | **21,306 bps** (~21.3 kbps) |
| **Strong Signal** | $30 \text{ dB}$ | $1000.0$ | $\approx 9.97 \text{ bits/symbol}$ | **31,895 bps** (~31.9 kbps) |
| **Pristine / High SNR** | $40 \text{ dB}$ | $10,000.0$ | $\approx 13.29 \text{ bits/symbol}$ | **42,520 bps** (~42.5 kbps) |

---

### Practical Application (e.g., VARA FM)

A 3200 Hz passband roughly corresponds to a **standard voice-grade
SSB/FM radio channel** (such as VARA FM Narrow mode):

* At a typical strong signal level of **$25 \text{ dB}$ SNR**
  ($\text{SNR} \approx 316.2$):

$$C = 3,200 \times \log_2(1 + 316.2) = 3,200 \times 8.31 \approx
\mathbf{26,590 \text{ bps}}$$

Real-world digital protocols (like VARA FM Narrow, which reaches
roughly $12,000 \text{ bps}$) achieve around **40% to 50%** of the
theoretical Shannon limit due to Forward Error Correction (FEC)
overhead, symbol training preambles, phase jitter, and filter edge
roll-offs.

## Half-Duplex Turnaround Time Analysis

To determine the ideal MAC quiet period after a max-frame burst, we
have to account for the physical delays in the hardware chain:

    Local RX Settling: ~50 ms (Time for your TH-D75 to drop PTT and
    stabilize the receiver).

    Remote Squelch & DSP: ~30 ms (Time for the receiving radio to open
    its squelch and GNU Radio to decode the frame).

    Remote PTT Assertion: ~80 ms (Time for the remote radio's internal
    relays to physically switch to TX).

    Remote Preamble: ~20 ms (Time to send the synchronization bytes).

    Buffer Margin: ~120 ms (Safety padding for OS context switching
    and ALSA buffering).

Recommendation: A minimum 300 ms MAC Cooldown after dropping PTT. If
you hit your burst_limit_, you stop transmitting, drop PTT, and ignore
your local TX queue for 300 ms to give the other radio a chance to
seize the channel.

## Trellis FSM

A Trellis FSM (Finite State Machine) file defines the state-transition
rules and coding memory of a convolutional encoder used in Trellis
Coded Modulation (TCM). It tells GNU Radio's `gr-trellis` module how
input bit sequences transition between internal memory states and map
to output constellation indices.

**What is a Trellis FSM File?**

In `gr-trellis`, an FSM file is a text file (or in-memory structure)
representing the encoder's state machine. It contains four core
components:

* **$I$ (Input Alphabet Size):** Number of possible input symbols
  ($2^k$, where $k$ is the number of unencoded data bits per symbol).
* **$S$ (Number of States):** Total internal memory states ($2^m$,
  where $m$ is the number of memory bits).
* **$O$ (Output Alphabet Size):** Number of encoded output symbols
  ($2^{k+1}$).
* **Next State (NS) & Output Symbol (OS) Tables:** Matrices mapping
  every `(current_state, input_symbol)` pair to its `next_state` and
  corresponding `output_symbol_index`.

**How Do We Generate One?**

You can generate an `.fsm` file using Python helper scripts or define
it directly in C++ memory:

1. **Using Python / GNU Radio Utilities:**
GNU Radio provides Python bindings in `gnuradio.trellis` to generate FSMs from generator matrix polynomials or systematic matrix definitions, which can then be saved to disk:
```python
from gnuradio import trellis
# Example: Generate an FSM from generator polynomials
fsm = trellis.fsm(k, n, [poly1, poly2, ...])
fsm.write_fsm_file("tcm_16qam.fsm")

```


2. **Direct C++ Instantiation (Recommended for Daemons):**
To avoid managing external text files on disk in your `75fasmod` daemon, you can instantiate `gr::trellis::fsm` directly in C++ using `std::vector<int>` for the NS and OS tables:
```cpp
#include <gnuradio/trellis/fsm.h>

// fsm(I, S, O, NS_table, OS_table)
gr::trellis::fsm tcm_fsm(I, S, O, ns_matrix, os_matrix);

```



**Do You Need One for Each Constellation?**

**Yes.** Standard Ungerboeck TCM pairs a rate $k/(k+1)$ convolutional
encoder with a constellation of size $2^{k+1}$. Because the input and
output symbol dimensions scale exponentially with the constellation
order, each modulation level requires its own distinct FSM:

* **16-QAM TCM (Rate 3/4):** $k=3$ data bits $\rightarrow$ $I = 2^3 = 8$, $O = 2^4 = 16$ constellation points.
* **64-QAM TCM (Rate 5/6):** $k=5$ data bits $\rightarrow$ $I = 2^5 = 32$, $O = 2^6 = 64$ constellation points.
* **256-QAM TCM (Rate 7/8):** $k=7$ data bits $\rightarrow$ $I = 2^7 = 128$, $O = 2^8 = 256$ constellation points.

For an adaptive modem, you will define a dedicated FSM object and
matching constellation map for each constellation order you plan to
support.

## QAM limitations

512-QAM is not practical over the TH-D75's analog FM audio
path. 512-QAM requires an Error Vector Magnitude (EVM) better than -33
dB (an SNR > 35 dB) and near-zero phase jitter, whereas FM
discriminator circuits, group-delay distortion, and 16-bit audio
codecs cap your usable channel SNR to roughly 22–28 dB. This makes
**256-QAM the absolute theoretical ceiling** and **64-QAM the
realistic high-throughput target**.

### Constellation & Trellis FSM Breakdown

Standard 2D Ungerboeck TCM uses an 8-state ($S = 8$) or 16-state
convolutional encoder where the input alphabet size is $I = 2^k$ (for
$k$ information bits) and the output symbol alphabet size is $O =
2^{k+1}$.

| Modulation | Grid Construction | Info Bits ($k$) | Constellation Points ($2^{k+1}$) | FSM Input ($I$) | FSM Output ($O$) |
| --- | --- | --- | --- | --- | --- |
| **16-QAM** | $4 \times 4$ Square | 3 bits/symbol | 16 points | 8 | 16 |
| **32-QAM** | $6 \times 6$ Cross (1 point removed per corner) | 4 bits/symbol | 32 points | 16 | 32 |
| **64-QAM** | $8 \times 8$ Square | 5 bits/symbol | 64 points | 32 | 64 |
| **128-QAM** | $12 \times 12$ Cross (4 points removed per corner) | 6 bits/symbol | 128 points | 64 | 128 |
| **256-QAM** | $16 \times 16$ Square | 7 bits/symbol | 256 points | 128 | 256 |

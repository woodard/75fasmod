# Radio Notes

## AF vs IF mode
When using a radio like the Kenwood TH-D75 for digital modes like FT8,
the difference between **AF (Audio Frequency) mode** and **IF
(Intermediate Frequency) mode** is essentially the difference between
sending "processed music" versus sending "the raw digital file."

Here is the practical breakdown:

### 1. AF Mode (Audio Frequency)
In AF mode, the radio does all the "work" of decoding the signal
first. It takes the radio waves, extracts the audio, and then sends
that audio signal over the USB cable to your computer.

*   **How it works:** Radio $\rightarrow$ Demodulation $\rightarrow$
    **Audio Processing (Filters/AGC)** $\rightarrow$ USB $\rightarrow$
    PC.
*   **The Downside:** The audio signal has been passed through the
    radio's internal audio circuitry. This includes things like **AGC
    (Automatic Gain Control)**, which changes the volume, and **Audio
    Filters**, which might cut off certain frequencies.
*   **Impact on FT8:** FT8 relies on extremely precise frequency
    shifts (measured in just a few Hertz). If the radio's audio
    filters or AGC "smear" or compress the signal, the computer may
    struggle to decode the weak, precise tones, leading to failed
    decodes.

### 2. IF Mode (Intermediate Frequency / Data Mode)
In IF mode, the radio bypasses the audio processing stage. It sends
the "raw" digital data stream (the signal as it exists in the
intermediate frequency stage) directly over the USB cable to your
computer.

*   **How $\text{it}$ works:** Radio $\rightarrow$ Demodulation
    $\rightarrow$ **Raw Digital Stream** $\rightarrow$ USB
    $\rightarrow$ PC.
*   **The Advantage:** The signal is "pure." There is no radio-induced
    distortion, no volume fluctuations from AGC, and no frequency
    cutting from audio filters. Your computer (running WSJT-X)
    receives the exact same signal that the radio is receiving.
*   **Impact on FT8:** This is the "gold standard" for digital
    modes. Because the signal is unprocessed, the software can see the
    cleanest possible version of the FT8 tones, significantly
    increasing your ability to decode very weak signals.

### Summary Comparison

| Feature | **AF Mode (Audio)** | **IF Mode (Data/Raw)** |
| :--- | :--- | :--- |
| **Signal Quality** | Processed, potentially distorted | Raw, pure, and clean |
| **Audio Filtering** | Subject to radio's audio filters | Bypasses radio's audio filters |
| **AGC Impact** | Volume changes can affect decoding | No impact from radio AGC |
| **Complexity** | Works with any basic audio setup | Requires a radio with a digital USB interface |
| **Best For...** | SSB, CW, and simple voice communication | **FT8, FT4, and all digital modes** |

**The Bottom Line:** For FT8, you should **always use IF mode** if
your radio supports it. It eliminates the radio's audio hardware as a
"weak link" in your signal chain.

### Setting the correct mode

Based on the search results from official Kenwood documentation and
community discussions, the setting you are looking for is likely
located in the following menu:

*   **Menu No. 102 (USB Out Select)**

According to the Kenwood product information for the TH-D75A/E, this
specific menu item is used to configure the USB output behavior. While
the snippet does not explicitly list the exact labels (e.g., "AF" vs
"IF"), it identifies **Menu No. 102** as the place to check when
managing the USB interface and its output modes.

**Summary for your configuration:**
1.  Navigate to the radio's menu system.
2.  Locate **Menu No. 102**.
3.  Look for the option within that menu that allows you to toggle
    between the different output types (likely **AF/Audio** for
    standard audio or **IF/Data** for digital modes).

*Note: If you are using older guides that reference the TH-D74, they
may mention Menu 980, but for the TH-D75, the official documentation
points to Menu 102.*

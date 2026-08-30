# Radio Notes

## Frustrations about the radio
Overall frustrations:
* Only supports legacy Bluetooth. Not modern Bluetooth Low Energy.
  * Only supports the Bluetooth HSP. This is a legacy Bluetooth
    profile and isn't supported on the iPhone or newer headsets. It
    would be nicer if it supported the more modern HFP. Even though it
    is only mono.
  * Only Supports Bluetooth SPP. This is also a legacy Bluetooth
    profile and only supports connected mode. There are newer profiles
    which take less energy.
* Only does USB Audio out. It would be better if the sound card did
  both Audio in and out.
* Even though it has a IF tap that can capture a whole 15  


### USB Audio
The USB Audio feature on the TH-D75A/E supports audio output only.
* The supported output format is 48 kHz, 16-bit, and monaural audio.
* USB Audio outputs the same audio sound as the speaker output.
* The output level of USB Audio can be adjusted in Menu No. 91A.
* Audio sound is output from the USB even when a speaker microphone or earphone is connected.
* When a Bluetooth headset is connected, USB Audio becomes a port used exclusively for the input and output of the Bluetooth headset.
[Operating Tips 5.13.2]

It would be so much nicer if they did full USB in and out. Since it is
a dual band radio, Even though the signal is mono, provide

### Bluetooth
[Operating Tips 5.12.1] The TH-D75A/E does not support Bluetooth Low
Energy (BLE). Therefore, it is not compatible with applications or
devices that require BLE. It only supports communication via the HSP
and SPP profiles.

This includes the iPhone.

#### HSP (Headset Profile)
By connecting the TH-D75A/E to a Bluetooth-compatible headset device,
the microphone and earphone of the headset can be used for making
voice calls.

PTT on the transceiver unit or speaker microphone is used to start
transmission. (Transmission cannot be made directly from the Bluetooth
headset, except for VOX operation, as PTT is not available via the
Bluetooth headset.)

To adjust the volume level when a Bluetooth device is connected, do so
using the volume control of the headset. The sensitivity of the
microphone can be adjusted in Menu No. 112.  

Note: Audio sound cannot be output from the USB or SP port when a
Bluetooth headset is connected. HFP (Hands-Free Profile) is not
supported.

#### SPP (Serial Port Profile)
By pairing the TH-D75A/E with a PC and assigning a virtual serial
port, it is possible to carry out serial communication with the PC
wirelessly. Doing so enables wireless operation of APRS applications
such as UI-View32 and MCP-D75. Also, serial communication with a PC is
possible while a Bluetooth headset is being connected.  During virtual
serial communication via Bluetooth, configuration of the baud rate is
not necessary to ensure communication at the optimal speed. If it is
necessary to configure the baud rate in the PC application program in
use, select any of the available options.  Bluetooth ON (Bluetooth
icon appears) Bluetooth (connected) icon lights up Bluetooth devices
that are currently connected will appear in Menu No. 933.  Note:

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

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
	
You cannot transmit when you are in IF mode.

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

However you cannot set menu item 102 when the radio is in dual
mode. You can't escape dual mode unless you have the TNC off.

Method 1: Query Dual VFO State via Hamlib Hamlib exposes multi-receiver capabilities through VFO management.
get_vfo_list / \get_vfo_list: Query the list of available VFOs.
get_vfo (v): Queries the currently active VFO (returns VFOA, VFOB, Main, Sub, etc.).
u DUAL / u DUAL_WATCH: If supported by the specific backend release, function state commands check specific operational flags.
You can test function states in rigctl interactively:
rigctl -m <TH-D75_model_id> -r /dev/ttyUSB0 u DUAL


Method 2: Query Active Frequency/VFO Information When the radio is in Dual Watch mode, both Band A and Band B are active simultaneously. You can query parameter states across VFOs:
# Query frequency on VFO A
rigctl -m <model_id> -r /dev/ttyUSB0 get_freq VFOA

# Query frequency on VFO B
rigctl -m <model_id> -r /dev/ttyUSB0 get_freq VFOB


If the backend accepts queries for Sub/VFO B without returning a target error, both bands are actively initialized.
Method 3: Direct Native CAT Command (Most Reliable) Kenwood radios (including the TH-D74 and TH-D75) use standard Kenwood serial ASCII commands. You can pass raw serial commands through Hamlib using w or W (send_cmd / send_cmd_rx):
BC Command (Band Control / Dual Status): Sending BC to a Kenwood handheld returns the status of Band A and Band B mode (e.g., whether single-band mode or dual-band mode is toggled).
# Pass raw command 'BC;' to the TH-D75 via Hamlib
rigctl -m <model_id> -r /dev/ttyUSB0 W "BC;"



User prompt: What are the native Kenwood ASCII CAT commands for toggling dual watch and switching VFOs on the TH-D75?
Response: Kenwood uses a consistent ASCII command structure across its handheld line (including the TH-D72, TH-D74, and TH-D75). Commands are sent over the serial port (USB or Bluetooth) in ASCII format followed by a carriage return (\r or ;).
1. Toggling Dual Mode / Single Band Mode
Command: BC (Band Control)
Query State:
BC

Response format: BC x,y (where x is the Control/Main band and y is the Dual-Band state).
Set Dual Mode ON (Dual Watch Active):
BC 0,1    (Sets Control to Band A, Dual Mode ON)

BC 1,1    (Sets Control to Band B, Dual Mode ON)


Set Single Band Mode (Dual Watch Disabled):
BC 0,0    (Sets Single Band Mode operating solely on Band A)

BC 1,0    (Sets Single Band Mode operating solely on Band B)


2. Switching Active VFO / Control Band
To change which band is active for transmitting or active control without altering the dual-watch state:
Command: BC or TXB (Transmit Band)
Switch Control Band to Band A (VFO A):
BC 0


Switch Control Band to Band B (VFO B):
BC 1


Alternatively, to switch the transmit target directly:
Set TX Band to A: TXB 0
Set TX Band to B: TXB 1
3. Switching Between VFO Mode and Memory Mode
To switch between frequency/VFO mode and Memory/Call mode for the selected band:
Command: VFO and MR
Set Active Band to VFO Mode:
VFO


Set Active Band to Memory Mode:
MR


Set Active Band to Call Channel:
CALL


Sending via Hamlib (rigctl)
To send these directly using Hamlib's pass-through command W (send_cmd_rx):
# Query Dual Mode Status
rigctl -m <model_id> -r /dev/ttyUSB0 W "BC;"

# Enable Dual Mode with Band A Control
rigctl -m <model_id> -r /dev/ttyUSB0 W "BC 0,1;"

# Switch to VFO B as active control
rigctl -m <model_id> -r /dev/ttyUSB0 W "BC 1;"



User prompt: If I am trying to do this from C/C++ what function would I call?
Response: To execute these native Kenwood CAT commands (BC;, BC 0,1;, etc.) directly through Hamlib's C/C++ API, use the rig_has_scan / rig_raw_cmd or rig_send_raw_cmd functions depending on your specific Hamlib version.
The standard high-level Hamlib API function for sending raw serial pass-through commands is rig_send_raw_cmd() (or rig_has_scan / rig_pass_through in older releases).
C/C++ Implementation Example
#include <stdio.h>
#include <string.h>
#include <hamlib/rig.h>

int main() {
    RIG *my_rig;
    rig_model_t my_model = RIG_MODEL_KENWOOD_THD74; // Or TH-D75 model ID when available
    int res;

    rig_set_debug(RIG_DEBUG_NONE);
    my_rig = rig_init(my_model);
    
    // Configure serial port settings
    strncpy(my_rig->state.rigport.pathname, "/dev/ttyUSB0", sizeof(my_rig->state.rigport.pathname));
    my_rig->state.rigport.parm.serial.rate = 9600;

    res = rig_open(my_rig);
    if (res != RIG_OK) {
        printf("Failed to open rig: %s\n", rigerror(res));
        return 1;
    }

    // --- Sending Raw CAT Command via Hamlib C API ---
    const char *cmd = "BC;\r";       // Kenwood command to query Dual/Band status
    char response[64] = {0};
    int response_len = 0;

    /*
     * rig_send_raw_cmd signature:
     * rig_send_raw_cmd(RIG *rig, const char *in_buf, int in_len, 
     *                  char *out_buf, int out_len, const char *term)
     */
    res = rig_send_raw_cmd(
        my_rig,             // Rig handle
        cmd,                // Raw ASCII string to send
        strlen(cmd),        // Input command length
        response,           // Buffer to store radio's response
        sizeof(response),   // Maximum response buffer size
        "\r"                // Response terminator character expected
    );

    if (res == RIG_OK) {
        printf("Radio Response: %s\n", response);
    } else {
        printf("Failed to send command: %s\n", rigerror(res));
    }

    rig_close(my_rig);
    rig_cleanup(my_rig);
    return 0;
}


Key Hamlib C API Functions Reference
rig_send_raw_cmd(): Sends raw command bytes to the rig hardware port and waits for a response up to the specified terminator character.
rig_set_vfo(): High-level alternative to switch active VFO directly using Hamlib enums (e.g., rig_set_vfo(my_rig, RIG_VFO_A, RIG_VFO_B)).
rig_get_vfo(): High-level function to retrieve current active VFO state (RIG_VFO_MAIN, RIG_VFO_SUB, RIG_VFO_A, etc.).

User prompt: using hamlib or the raw kenwood CAT commands how do you find out if the TNC is on? i.e. either in APRS KISS12 or KISS96 mode?
Response: To check if the TNC is active and what mode it is running in on a Kenwood handheld (like the TH-D75 or TH-D74), you query the TN (TNC Status) command over native Kenwood CAT commands.
Hamlib does not expose a high-level rig_get_aprs_mode() abstraction for internal TNC states, so sending raw CAT commands via W / rig_send_raw_cmd() is the standard approach.
1. Querying TNC Status (TN Command)
Send the raw command TN; to the radio.
Command:
TN;


Returned Responses:
TN 0: TNC is OFF.
TN 1: TNC is ON in APRS Mode (Internal Packet/APRS active).
TN 2: TNC is ON in KISS Mode (1200 bps).
TN 3: TNC is ON in KISS Mode (9600 bps).
(Note: Depending on firmware revisions, TN 1 is standard internal APRS mode, while TN 2 and TN 3 designate KISS 1200 and KISS 9600 respectively).
2. C / C++ Implementation with Hamlib
Use rig_send_raw_cmd() to issue TN; and parse the integer returned after TN :
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hamlib/rig.h>

enum TNCMode {
    TNC_OFF = 0,
    TNC_APRS = 1,
    TNC_KISS_1200 = 2,
    TNC_KISS_9600 = 3,
    TNC_UNKNOWN = -1
};

TNCMode check_tnc_status(RIG *my_rig) {
    const char *cmd = "TN;\r";
    char response[32] = {0};

    int res = rig_send_raw_cmd(
        my_rig,
        cmd,
        strlen(cmd),
        response,
        sizeof(response) - 1,
        "\r"
    );

    if (res != RIG_OK) {
        printf("Error querying TNC: %s\n", rigerror(res));
        return TNC_UNKNOWN;
    }

    // Response format expected: "TN 0", "TN 1", "TN 2", or "TN 3"
    if (strncmp(response, "TN ", 3) == 0) {
        int status = atoi(&response[3]);
        return (TNCMode)status;
    }

    return TNC_UNKNOWN;
}

int main() {
    // Standard Hamlib init sequence ...
    RIG *my_rig = rig_init(RIG_MODEL_KENWOOD_THD74); // Replace with TH-D75 model ID when available
    strncpy(my_rig->state.rigport.pathname, "/dev/ttyUSB0", sizeof(my_rig->state.rigport.pathname));
    my_rig->state.rigport.parm.serial.rate = 9600;

    if (rig_open(my_rig) != RIG_OK) return 1;

    TNCMode mode = check_tnc_status(my_rig);

    switch (mode) {
        case TNC_OFF:
            printf("TNC is OFF\n");
            break;
        case TNC_APRS:
            printf("TNC is active in Internal APRS Mode\n");
            break;
        case TNC_KISS_1200:
            printf("TNC is active in KISS 1200 bps Mode\n");
            break;
        case TNC_KISS_9600:
            printf("TNC is active in KISS 9600 bps Mode\n");
            break;
        default:
            printf("Could not determine TNC status.\n");
            break;
    }

    rig_close(my_rig);
    rig_cleanup(my_rig);
    return 0;
}


3. Setting the TNC State via CAT
To toggle or switch TNC modes directly programmatically:
Turn OFF TNC: TN 0;
Enable APRS Mode: TN 1;
Enable KISS 1200 Mode: TN 2;
Enable KISS 9600 Mode: TN 3;

User prompt: can you cite a source where you are finding all these CAT commands?
Response: The primary sources for these serial CAT commands come directly from Kenwood official technical documentation, Hamlib source code, and the APRS/packet radio developer community:
Kenwood Official Command Specifications:
Kenwood TH-D74/TH-D75 Command Reference Protocols: Kenwood publishes official PC Control / Command Reference Manuals for its digital transceivers (such as the TH-D72, TH-D74, and TH-D75) outlining the serial control syntax over USB/Bluetooth COM ports.
Commands like BC (Band Control), TXB (Transmit Band), VFO, and TN (TNC Control) are defined in these protocol guides.
Hamlib Source Repository (Backend Drivers):
The C drivers in the Hamlib GitHub repository (specifically kenwood/kenwood.c, kenwood/thd72.c, and kenwood/thd74.c) implement these specific raw byte sequences.
You can inspect how Hamlib interacts with Kenwood handhelds directly in the codebase under rigs/kenwood/.
TNC / APRS Developer Documentation:
The TN command for toggling internal TNC state (0 = Off, 1 = APRS Mode, 2 = KISS 1200, 3 = KISS 9600) is heavily documented in open-source APRS software projects (such as Dire Wolf, Xastir, and YAAC) where developers issue direct setup commands to Kenwood hardware over virtual serial ports.

User prompt: What CAT command allows you to figure out which band the TNC is currently using if it is on?
Response: To determine which band (Band A or Band B) the TNC is assigned to on Kenwood handhelds, use the TNC command (or query parameter TN / TNC status depending on parameter flags) or query the BC (Band Control) state.
1. The TNC Command
Querying TNC returns the current TNC band allocation alongside its operational state.
Query Command:
TNC


Response Format:
TNC x,y


x (TNC Mode / State):
0 = Off
1 = APRS / Packet Mode
2 = KISS 1200 bps
3 = KISS 9600 bps
y (TNC Band Target):
0 = Band A (Main / Top)
1 = Band B (Sub / Bottom)
Example: A response of TNC 1,0 indicates the TNC is turned ON in APRS mode and attached to Band A. A response of TNC 2,1 means KISS 1200 is running on Band B.
2. Switching the TNC Band via CAT
If you need to programmatically assign the TNC to a specific band:
Set TNC to Band A:
TNC 1,0    (Sets APRS/TNC mode ON on Band A)


Set TNC to Band B:
TNC 1,1    (Sets APRS/TNC mode ON on Band B)


3. C / C++ Implementation Example
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hamlib/rig.h>

void get_tnc_band(RIG *my_rig) {
    const char *cmd = "TNC;\r";
    char response[32] = {0};

    int res = rig_send_raw_cmd(
        my_rig,
        cmd,
        strlen(cmd),
        response,
        sizeof(response) - 1,
        "\r"
    );

    if (res == RIG_OK && strncmp(response, "TNC ", 4) == 0) {
        int mode = 0;
        int band = 0;
        
        // Parse "TNC x,y"
        if (sscanf(response, "TNC %d,%d", &mode, &band) == 2) {
            if (mode == 0) {
                printf("TNC is currently OFF.\n");
            } else {
                printf("TNC Mode: %d | Assigned Band: Band %s\n", 
                       mode, (band == 0) ? "A" : "B");
            }
            return;
        }
    }

    printf("Failed to query TNC band status: %s\n", response);
}

In Hamlib, both **`rig_send_raw()`** and **`rig_send_raw_cmd()`** allow low-level pass-through access to the rig's communication port, but they serve fundamentally different network/hardware purposes and handle communication framing differently.

---

### **1. `rig_send_raw_cmd()` (Text/CAT Pass-Through)**

Use **`rig_send_raw_cmd()`** when sending high-level ASCII CAT commands to radios that use text-based command interfaces (like Kenwood, Elecraft, or Ten-Tec).

```c
int rig_send_raw_cmd(RIG *rig, const char *in_buf, int in_len,
                     char *out_buf, int out_len, const char *term);

```

* **Purpose:** It writes a text command to the transceiver and automatically reads back the response, stopping when it sees a specific termination character (like `\r` or `;`) or reaches a timeout.
* **Termination Handling:** It uses the `term` string parameter to detect the end of the radio's response (e.g., `";"` for Kenwood or Yaesu CAT commands).
* **Expected Usage:** Sending text string queries (such as `"BC;"` or `"TN;"`) where you expect an ASCII formatted string returned in `out_buf`.

---

### **2. `rig_send_raw()` (Raw Binary Byte I/O)**

Use **`rig_send_raw()`** when sending raw binary packets or working with byte-oriented protocols (like Icom CI-V, Yaesu binary CAT protocols, or raw serial streams).

```c
int rig_send_raw(RIG *rig, const unsigned char *in_buf, int in_len,
                 unsigned char *out_buf, int out_len);

```

* **Purpose:** It performs a direct write/read on the serial or network port buffer without searching for ASCII line terminators or string boundaries.
* **Termination Handling:** It relies strictly on reading a predetermined byte length (`out_len`) or stopping when the underlying port read buffer clears out / times out.
* **Expected Usage:** Sending raw byte arrays, binary hex frames, or custom hardware control commands that contain non-printable characters or `0x00` (NUL) bytes.

---

### **Summary**

| Feature | `rig_send_raw_cmd()` | `rig_send_raw()` |
| --- | --- | --- |
| **Data Format** | Null-terminated or text ASCII strings | Raw binary byte arrays (`unsigned char*`) |
| **Response Terminator** | Looks for explicit terminator string (e.g., `"\r"` or `";"`) | Reads fixed number of bytes or until timeout |
| **Typical Target Radios** | Kenwood (`BC;`), Elecraft (`FA;`) | Icom CI-V hex frames (`0xFE 0xFE...`), Yaesu binary |
| **Best Choice For TH-D75** | **Yes** (For standard text commands) | Only if interfacing with custom binary streams |

AI stands for Auto Information.

In the Kenwood CAT (Computer Aided Transceiver) protocol, the AI command controls whether the radio spontaneously reports its status changes to the computer.

Here is how the modes work:

    AI 0 (Auto Information OFF): The radio speaks only when spoken to. It will only send data over the serial port if you explicitly ask for it (e.g., if you send FQ, it replies with FQ).

    AI 1 or AI 2 (Auto Information ON): The radio becomes chatty. If you twist the tuning knob, press the PTT button, or if a signal opens the squelch, the radio automatically blasts that new state out over the serial port without waiting for a query.

Why we are using AI 0 in the destructor:
If a previous application (like an APRS client or another Hamlib script) crashed or exited while Auto Information was turned on, the radio will continue spewing spontaneous updates onto the serial line. When your test script opens the port, those unprompted updates collide with the responses Hamlib actually expects, causing the offset-by-one -RIG_EPROTO errors we were fighting.

Sending AI 0 immediately silences the radio, ensuring the serial line is perfectly predictable and strictly call-and-response.
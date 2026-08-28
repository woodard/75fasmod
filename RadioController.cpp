#include "RadioController.hpp"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

RadioController::RadioController(rig_model_t model, const std::string& port)
    : model_(model), port_(port), rig_(nullptr), 
      orig_mode_(RIG_MODE_NONE), orig_width_(0), orig_menu_102_(-1), orig_power_(-1) {}

RadioController::~RadioController() {
    if (rig_) {
        std::cout << "[RIG] Shutting down. Restoring original radio settings...\n";
        set_ptt(false); 

        if (orig_menu_102_ >= 0) {
            kenwood_menu_set(102, orig_menu_102_);
        }

        // 1. Restore the original power level
        if (orig_power_ >= 0) {
            kenwood_power_set(orig_power_);
        }

        if (orig_mode_ != RIG_MODE_NONE) {
            rig_set_mode(rig_, RIG_VFO_CURR, orig_mode_, orig_width_);
        }

        rig_close(rig_);
        rig_cleanup(rig_);
    }
}

bool RadioController::initialize() {
    rig_ = rig_init(model_);
    if (!rig_) return false;
    strncpy(rig_->state.rigport.pathname, port_.c_str(), FILPATHLEN - 1);
    
    if (rig_open(rig_) != RIG_OK) {
        std::cerr << "Error: Could not open radio on " << port_ << "\n";
        return false;
    }

    std::cout << "[RIG] Backing up current radio state...\n";

    // --- 1. BACKUP STATE ---
    // Backup current operating mode (e.g., FM, Voice, etc)
    rig_get_mode(rig_, RIG_VFO_CURR, &orig_mode_, &orig_width_);
    
    // Backup Menu 102 (USB Out Select). Kenwood TH-D75 uses EX commands for menus.
    orig_menu_102_ = kenwood_menu_get(102);
    
    // --- 2. SET MODEM STATE ---
    std::cout << "[RIG] Configuring radio for high-speed modem operation...\n";
    
    // Set standard Hamlib mode to Packet FM with 9600 baud passband
    rig_set_mode(rig_, RIG_VFO_CURR, RIG_MODE_PKTFM, 9600);
    
    // Backup current transmit power level
    orig_power_ = kenwood_power_get();

    // Force Menu 102 to '1' (Detect). 
    // This taps the direct discriminator, bypassing the 6dB/oct de-emphasis filter.
    kenwood_menu_set(102, 1);

    return true;
}

// ... [Keep set_frequency, set_ptt, get_dcd from previous steps] ...

// --- Private Helpers for Kenwood Specific Features ---

int RadioController::kenwood_menu_get(int menu_num) {
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "EX%03d;", menu_num);
    
    // Send command
    rig_send_raw(rig_, (unsigned char*)cmd, strlen(cmd));
    
    // Wait briefly for radio processor to answer
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Read response. Expected format: "EX102,0;" or "EX102,1;"
    char buf[64] = {0};
    int bytes = rig_read_raw(rig_, (unsigned char*)buf, sizeof(buf)-1);
    
    if (bytes > 0) {
        std::string resp(buf);
        size_t comma = resp.find(',');
        size_t semi = resp.find(';');
        if (comma != std::string::npos && semi != std::string::npos) {
            try {
                return std::stoi(resp.substr(comma + 1, semi - comma - 1));
            } catch (...) {
                return -1;
            }
        }
    }
    return -1; // Failed to parse
}

void RadioController::kenwood_menu_set(int menu_num, int value) {
    if (value < 0) return; // Skip if invalid value
    
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "EX%03d,%d;", menu_num, value);
    
    rig_send_raw(rig_, (unsigned char*)cmd, strlen(cmd));
    
    // Give the radio firmware time to apply the setting to its matrix switches
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool RadioController::set_frequency(double freq_mhz) {
    freq_t freq_hz = static_cast<freq_t>(freq_mhz * 1000000.0);
    return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
}

bool RadioController::set_ptt(bool transmit) {
    ptt_t ptt_state = transmit ? RIG_PTT_ON : RIG_PTT_OFF;
    return rig_set_ptt(rig_, RIG_VFO_CURR, ptt_state) == RIG_OK;
}

bool RadioController::get_dcd(bool& is_squelch_open) {
    dcd_t dcd_status;
    if (rig_get_dcd(rig_, RIG_VFO_CURR, &dcd_status) == RIG_OK) {
        is_squelch_open = (dcd_status == RIG_DCD_ON);
        return true;
    }
    return false;
}

bool RadioController::set_power_level(const std::string& level) {
    // Convert input string to uppercase for easy comparison
    std::string lvl = level;
    for (auto &c : lvl) c = std::toupper(c);

    int val = -1;
    if (lvl == "H")       val = 0;
    else if (lvl == "M")  val = 1;
    else if (lvl == "L")  val = 2;
    else if (lvl == "EL") val = 3;
    else {
        std::cerr << "Error: Invalid power level '" << level << "'. Use EL, L, M, or H.\n";
        return false;
    }

    std::cout << "[RIG] Setting TX power to " << lvl << "...\n";
    kenwood_power_set(val);
    return true;
}

// ... [Keep kenwood_menu_get and kenwood_menu_set] ...

// --- Power Control Implementation ---

int RadioController::kenwood_power_get() {
    rig_send_raw(rig_, (const unsigned char*)"PC;", 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    char buf[32] = {0};
    int bytes = rig_read_raw(rig_, (unsigned char*)buf, sizeof(buf)-1);
    
    if (bytes > 0) {
        std::string resp(buf);
        size_t pc_pos = resp.find("PC");
        size_t semi = resp.find(';');
        
        // Expecting "PCx;" where x is 0, 1, 2, or 3
        if (pc_pos != std::string::npos && semi != std::string::npos && semi > pc_pos + 2) {
            try {
                return std::stoi(resp.substr(pc_pos + 2, semi - pc_pos - 2));
            } catch (...) {
                return -1;
            }
        }
    }
    return -1;
}

void RadioController::kenwood_power_set(int val) {
    if (val < 0 || val > 3) return;
    
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "PC%d;", val);
    
    rig_send_raw(rig_, (const unsigned char*)cmd, strlen(cmd));
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Allow relays to click
}

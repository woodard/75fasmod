#include <iostream>
#include <string>
#include <cstdlib>
#include <getopt.h>
#include <hamlib/rig.h>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -f, --freq <MHz>      Frequency to set in MHz (e.g., 144.390)\n"
              << "  -p, --port <device>   Serial port (default: /dev/ttyUSB0)\n"
              << "  -m, --model <id>      Hamlib rig model ID (default: 2 for generic Kenwood)\n"
              << "  -h, --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    double target_freq_mhz = 0.0;
    freq_t target_freq_hz = 0;
    std::string serial_port = "/dev/ttyUSB0";
    rig_model_t rig_model = RIG_MODEL_KENWOOD; // Model 2 (Generic Kenwood)

    // Parse command line arguments
    const char* const short_opts = "f:p:m:h";
    const option long_opts[] = {
        {"freq", required_argument, nullptr, 'f'},
        {"port", required_argument, nullptr, 'p'},
        {"model", required_argument, nullptr, 'm'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'f':
                target_freq_mhz = std::stod(optarg);
                target_freq_hz = static_cast<freq_t>(target_freq_mhz * 1000000.0);
                break;
            case 'p':
                serial_port = optarg;
                break;
            case 'm':
                rig_model = std::stoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (target_freq_hz == 0) {
        std::cerr << "Error: You must specify a target frequency in MHz.\n";
        print_usage(argv[0]);
        return 1;
    }

    // Initialize Hamlib
    std::cout << "Initializing Hamlib (Model ID: " << rig_model << ")...\n";
    RIG* my_rig = rig_init(rig_model);
    if (!my_rig) {
        std::cerr << "Error: Unknown rig model or memory allocation failure.\n";
        return 1;
    }

    // Configure the serial port
    strncpy(my_rig->state.rigport.pathname, serial_port.c_str(), FILPATHLEN - 1);
    
    // Open the connection to the radio
    std::cout << "Connecting to radio on " << serial_port << "...\n";
    if (rig_open(my_rig) != RIG_OK) {
        std::cerr << "Error: Could not open the radio connection. Check port and permissions.\n";
        rig_cleanup(my_rig);
        return 1;
    }

    // Set the frequency on the current active VFO
    std::cout << "Setting frequency to " << target_freq_mhz << " MHz...\n";
    int status = rig_set_freq(my_rig, RIG_VFO_CURR, target_freq_hz);
    if (status != RIG_OK) {
        std::cerr << "Error: Failed to set frequency. Hamlib error code: " << status << "\n";
    } else {
        std::cout << "Frequency set successfully!\n";
    }

    // Clean up
    rig_close(my_rig);
    rig_cleanup(my_rig);

    return 0;
}
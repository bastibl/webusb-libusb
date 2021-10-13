#include <iostream>

extern "C" {
#include "libusb.h"
#include <rtl-sdr.h>
}

static rtlsdr_dev_t *dev = NULL;

int main() {
    std::cout << "Hello from WASM C++." << std::endl;

	uint32_t devs = rtlsdr_get_device_count();
    std::cout << "RTL-SDR device count: " << devs << std::endl;

    if (devs == 0) {
        std::cout << "No RTL-SDR found." << std::endl;
        return 1;
	}

    std::cout << "RTL Device name: " << rtlsdr_get_device_name(0) << std::endl;

	int r = rtlsdr_open(&dev, 0);
	if (r < 0) {
        std::cout << "Failed to open RTL-SDR" << std::endl;
        return 1;
	}

	r = rtlsdr_set_center_freq(dev, 100000000);
	if (r != 0) {
        std::cout << "Failed to set center freq" << std::endl;
        return 1;
	}

	r = rtlsdr_set_sample_rate(dev, 1800000);
	if (r != 0) {
        std::cout << "Failed to set sample rate" << std::endl;
        return 1;
	}

	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r < 0) {
        std::cout << "Failed to set agc mode" << std::endl;
        return 1;
	}

	r = rtlsdr_set_agc_mode(dev, 1);
	if (r < 0) {
        std::cout << "Failed to set agc mode" << std::endl;
        return 1;
	}

    std::cout << "RTL-SDR OPEN SUCCESSFUL" << std::endl;

    return 0;
}

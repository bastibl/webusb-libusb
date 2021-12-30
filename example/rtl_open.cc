#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <iostream>

using namespace emscripten;

extern "C" {
#include "libusb.h"
#include <rtl-sdr.h>
}

static rtlsdr_dev_t *dev = NULL;
static uint8_t buf[16 * 512];

val read_samples() {
    int read = 0;
    int r = rtlsdr_read_sync(dev, buf, 16 * 512, &read);

    if (r != 0) {
        std::cout << "Failed to read samples" << std::endl;
        return val(typed_memory_view(0, buf));
    }

    // std::cout << "bytes read: " << read << std::endl;
    // std::cout << "first samples " << +buf[0] << ", " << +buf[1] << ", "
    //           << +buf[2] << ", " << +buf[3] << ", " << +buf[4] << ", "
    //           << +buf[5] << ", " << std::endl;

    return val(typed_memory_view(read, buf));
}

void set_freq(uint32_t freq) {
    int r = rtlsdr_set_center_freq(dev, freq);
    if (r != 0) {
        std::cout << "Failed to set center freq" << std::endl;
    }
}

EMSCRIPTEN_BINDINGS(rtl_open) {
    function("read_samples", &read_samples);
    function("set_freq", &set_freq);
}

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

    r = rtlsdr_set_sample_rate(dev, 3200000);
    if (r != 0) {
        std::cout << "Failed to set sample rate" << std::endl;
        return 1;
    }

    r = rtlsdr_set_agc_mode(dev, 0);
    if (r != 0) {
        std::cout << "Failed to disable agc mode" << std::endl;
        return 1;
    }

    r = rtlsdr_set_tuner_gain(dev, 340);
    if (r != 0) {
        std::cout << "Failed to set gain" << std::endl;
        return 1;
    }

    r = rtlsdr_reset_buffer(dev);
    if (r != 0) {
        std::cout << "Failed to reset buffer" << std::endl;
        return 1;
    }

    std::cout << "RTL-SDR OPEN SUCCESSFUL" << std::endl;

    return 0;
}

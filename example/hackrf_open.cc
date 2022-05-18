#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <iostream>
#include <stdint.h>

using namespace emscripten;

extern "C" {
#include "libusb.h"
#include <hackrf.h>
}

#define BUF_LEN			262144
#define BUF_NUM			15

hackrf_device* device = NULL;
int8_t		**buf;

std::mutex buf_mutex;

uint32_t read_index;
uint32_t write_index;
uint32_t read_available;

void allocate_buffers() {
	buf = (int8_t * *) malloc( BUF_NUM * sizeof(int8_t *) );
	if ( buf ) {
		for ( unsigned int i = 0; i < BUF_NUM; ++i ) {
			buf[i] = (int8_t *) malloc( BUF_LEN );
		}
	}

    read_index = 0;
    write_index = 0;
    read_available = 0;
}

int rx_callback(hackrf_transfer *transfer)
{
	std::unique_lock<std::mutex> lock(buf_mutex);
    // std::cout << "HackRF rx_callback, available " << read_available << "  bytes " << transfer-> valid_length << "  read index " << read_index << "  write index " << write_index << std::endl;

    uint32_t index = write_index;
    memcpy(buf[write_index], transfer->buffer, transfer->valid_length);

    write_index = (write_index + 1) % BUF_NUM;

    read_available += 1;
    if (read_available > BUF_NUM) {
        std::cout << "rx_callback overrun" << std::endl;
        read_index = index;
        read_available = 1;
    }

	return 0;
}

val read_samples() {
	std::unique_lock<std::mutex> lock(buf_mutex);

    // std::cout << "HackRF reading samples, available " << read_available << "  read index " << read_index << "  write index " << write_index << std::endl;
	if (read_available == 0)
	{
        return val(typed_memory_view(0, buf[0]));
	}

    uint32_t index = read_index;
    read_index = (read_index + 1) % BUF_NUM;
    read_available--;

    return val(typed_memory_view(BUF_LEN, buf[index]));
}

void set_freq(uint32_t freq) {
    int result = hackrf_set_freq(device, freq);
    if (result != HACKRF_SUCCESS) {
        std::cout << "Failed to set center freq" << std::endl;
    }
}

EMSCRIPTEN_BINDINGS(hackrf_open) {
    function("read_samples", &read_samples);
    function("set_freq", &set_freq);
}

int main() {
    std::cout << "Hello from WASM C++." << std::endl;

    allocate_buffers();

	int result = hackrf_init();
	if( result != HACKRF_SUCCESS ) {
		fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name((enum hackrf_error)result), result);
		return EXIT_FAILURE;
	}

	result = hackrf_open_by_serial("", &device);
	if( result != HACKRF_SUCCESS ) {
		fprintf(stderr, "hackrf_open() failed: %s (%d)\n", hackrf_error_name((enum hackrf_error)result), result);
		return EXIT_FAILURE;
	}

    result = hackrf_set_freq(device, 2480000000ull);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_set_freq() failed: %s (%d)\n",
                hackrf_error_name((enum hackrf_error)result), result);
        return EXIT_FAILURE;
    }

    result = hackrf_set_sample_rate(device, 4e6);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_set_sample_rate() failed: %s (%d)\n",
                hackrf_error_name((enum hackrf_error)result), result);
        return EXIT_FAILURE;
    }

    result  = hackrf_set_amp_enable(device, 1);
    result |= hackrf_set_lna_gain(device, 32);
    result |= hackrf_set_vga_gain(device, 14);
    result |= hackrf_start_rx(device, rx_callback, NULL);
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_start_?x() failed: %s (%d)\n",
                hackrf_error_name((enum hackrf_error)result), result);
        return EXIT_FAILURE;
    }
    std::cout << "HackRF Open Successful" << std::endl;

    return 0;
}

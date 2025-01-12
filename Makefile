all: libusb external examples

PWD_DIR := $(shell pwd)
BUILD_DIR := $(PWD_DIR)/build

EM_DEBUG := -msse -msimd128 -gsource-map --profiling --source-map-base http://localhost:8000/ -fsanitize=undefined
EM_FLAGS := $(EM_DEBUG) -O3 -s USE_PTHREADS=1 -s WASM=1 -s 'ASYNCIFY_IMPORTS=["emscripten_receive_on_main_thread_js"]' -s ASYNCIFY_STACK_SIZE=10000 -s INITIAL_MEMORY=128MB -s PROXY_TO_PTHREAD=1
EM_OPTS := -I$(BUILD_DIR)/include/ -L$(BUILD_DIR)/lib/ -lusb-1.0 --bind -s ASYNCIFY $(EM_FLAGS)

CMAKE_EM_OPTS := -DCMAKE_CXX_FLAGS="$(EM_OPTS)" -DCMAKE_C_FLAGS="$(EM_OPTS)"
CMAKE_LIBUSB_OPTS := -DLIBUSB_INCLUDE_DIR=$(BUILD_DIR)/include -DLIBUSB_LIBRARIES=$(BUILD_DIR)/lib
CMAKE_LIBUSB_OPTS_RTL := -DLIBUSB_INCLUDE_DIRS=$(BUILD_DIR)/include -DLIBUSB_LIBRARIES=$(BUILD_DIR)/lib/libusb-1.0.a
CMAKE_INSTALL_OPTS := -DCMAKE_INSTALL_PREFIX=$(BUILD_DIR)

build_dir:
	mkdir -p build

clean:
	rm -fr build
	rm -fr external/airspyone_host/build
	rm -fr external/airspyhf/build
	rm -fr external/samurai/build
	rm -fr external/liquid-dsp/build
	rm -fr external/fftw3/build
	rm -fr libusb/build
	rm -fr audiocontext/build

#
# Build Libraries
#

audiocontext: build_dir
	mkdir -p audiocontext/build
	cd audiocontext/build && emcmake cmake $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd audiocontext/build && emmake make -j8
	cd audiocontext/build && emmake make install

libusb: build_dir
	mkdir -p libusb/build
	cd libusb/build && emcmake cmake $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd libusb/build && emmake make -j8
	cd libusb/build && emmake make install

libs: audiocontext libusb

#
# Build External
#

liquid: build_dir
	cd external/liquid-dsp && bash bootstrap.sh
	cd external/liquid-dsp && emconfigure ./configure --build=x86_64 --target=x86_64 --host=x86_64 -C CFLAGS='-O3' --prefix=$(BUILD_DIR) ..
	cd external/liquid-dsp && emmake make -j8
	cd external/liquid-dsp && emmake make install

samurai: build_dir airspy
	rm -fr external/samurai/build
	cd external/samurai && PKG_CONFIG_PATH=$(BUILD_DIR)/lib/pkgconfig meson . build --cross-file target/emscripten.txt --prefix $(BUILD_DIR)
	cd external/samurai/build && ninja
	cd external/samurai/build && ninja install

airspy: build_dir
	mkdir -p external/airspyone_host/build
	cd external/airspyone_host/build && emcmake cmake $(CMAKE_LIBUSB_OPTS) $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd external/airspyone_host/build && emmake make -j8
	cd external/airspyone_host/build && emmake make install

rtl-sdr: build_dir
	mkdir -p external/rtl-sdr/build
	cd external/rtl-sdr/build && PKG_CONFIG_PATH=$(BUILD_DIR)/lib/pkgconfig emcmake cmake $(CMAKE_LIBUSB_OPTS_RTL) $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd external/rtl-sdr/build && emmake make
	cd external/rtl-sdr/build && emmake make install

hackrf: build_dir
	mkdir -p external/hackrf/host/build
	cd external/hackrf/host/build && PKG_CONFIG_PATH=$(BUILD_DIR)/lib/pkgconfig emcmake cmake $(CMAKE_LIBUSB_OPTS_RTL) $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd external/hackrf/host/build && emmake make
	cd external/hackrf/host/build && emmake make install

airspyhf: build_dir
	mkdir -p external/airspyhf/build
	cd external/airspyhf/build && emcmake cmake $(CMAKE_LIBUSB_OPTS) $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd external/airspyhf/build && emmake make -j8 VERBOSE=1
	cd external/airspyhf/build && emmake make install

fftw3: build_dir
	mkdir -p external/fftw3/build
	cd external/fftw3/build && emcmake cmake -DENABLE_FLOAT=YES $(CMAKE_LIBUSB_OPTS) $(CMAKE_INSTALL_OPTS) $(CMAKE_EM_OPTS) ..
	cd external/fftw3/build && emmake make -j8 VERBOSE=1
	cd external/fftw3/build && emmake make install

external: airspy airspyhf samurai fftw3 liquid

#
# Build Examples
#

example_dir: build_dir
	mkdir -p build/example

libusb_list_devices: libusb example_dir
	em++ $(EM_OPTS) example/libusb_list_devices.cc -o build/example/libusb_list_devices.html

airspy_list_devices: libusb airspy example_dir
	em++ $(EM_OPTS) -lairspy example/airspy_list_devices.cc -o build/example/airspy_list_devices.html

airspy_stream: libusb airspy example_dir
	em++ $(EM_OPTS) -lairspy example/airspy_stream.cc -o build/example/airspy_stream.html

samurai_stream: libusb airspy airspyhf samurai
	em++ $(EM_OPTS) --std=c++17 -lairspy -lsamurai example/samurai_stream.cc -o build/example/samurai_stream.html

samurai_radio: libusb airspy samurai audiocontext #liquid
	em++ $(EM_OPTS) --std=c++17 -laudiocontext -lairspy -lairspyhf -lsamurai -lliquid example/samurai_radio.cc -o build/example/samurai_radio.html

samurai_console: #libusb airspy samurai audiocontext #liquid
	em++ $(EM_OPTS) --std=c++17 -s FULL_ES3=1 -s USE_SDL=2 -lfftw3f -laudiocontext -lairspy -lairspyhf -lsamurai -lliquid example/samurai_console.cc -o build/example/samurai_console.html

audiocontext_test: audiocontext
	em++ $(EM_OPTS) --std=c++17 -laudiocontext example/audiocontext_test.cc -o build/example/audiocontext_test.html

cyberradio: #libusb airspy samurai audiocontext #liquid
	em++ $(EM_OPTS) -s MODULARIZE=1 -s 'EXPORT_NAME="CyberRadioCore"' -s PROXY_TO_PTHREAD=0 --std=c++17 -laudiocontext -lairspy -lairspyhf -lsamurai -lliquid example/cyberradio.cc -o build/example/cyberradio.js

rtl_open: rtl-sdr example_dir
	em++ $(EM_OPTS) --bind -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' -lrtlsdr example/rtl_open.cc -o build/example/rtl_open.js

hackrf_open: hackrf example_dir
	em++ $(EM_OPTS) --bind -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' -s ALLOW_BLOCKING_ON_MAIN_THREAD=1 -Ibuild/include/libhackrf -lhackrf example/hackrf_open.cc -o build/example/hackrf_open.js
	cp build/example/hackrf* ${HOME}/src/futuresdr/examples/spectrum/assets/static/

examples: libusb_list_devices airspy_list_devices airspy_stream samurai_stream samurai_radio audiocontext_test cyberradio rtl_open

.PHONY: gui
gui:
	cd foo && wasm-pack build --target web --out-name foo --out-dir ../build/example --dev

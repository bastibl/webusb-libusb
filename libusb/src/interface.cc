#include <iostream>

#include <pthread.h>
#include <emscripten.h>
#include <emscripten/threading.h>

#include "libusb.h"
#include "interface.h"

#define DEBUG_TRACE

//
// Helper functions.
//

static _Atomic bool started = false;

void *WUSBThread(void*) {
#ifdef DEBUG_TRACE
    std::cout << "WebUSB Thread Started (" << pthread_self() << ")" << std::endl;
#endif
    started = true;
    emscripten_exit_with_live_runtime();
    std::cout << "WebUSB Thread Done" << std::endl;
    return NULL;
}

device_context* dc(libusb_device* dev) {
    return (device_context*)dev;
}

webusb_context* wc(libusb_context* ctx) {
    return (webusb_context*)ctx;
}

webusb_context* wc(libusb_device* dev) {
    return dc(dev)->ctx;
}

webusb_context* wc(libusb_device_handle* dev_handle) {
    return (webusb_context*)dev_handle;
}

//
// Proxied methods.
//

static webusb_context* _ctx = NULL;

int libusb_init(libusb_context** ctx) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif

    if(!_ctx) {
        std::cout << "creating webusb_context" << std::endl;
        _ctx = (webusb_context*)calloc(1, sizeof(webusb_context));

        if (pthread_create(&_ctx->worker, NULL, WUSBThread, nullptr) != 0)
            return LIBUSB_ERROR_NOT_SUPPORTED;

        while (!started)
            emscripten_sleep(100);

        std::cout << "webusb thread: " << _ctx->worker << std::endl;
        *ctx = (libusb_context*)_ctx;

        return emscripten_dispatch_to_thread_sync(_ctx->worker, EM_FUNC_SIG_II, _libusb_init, nullptr);

    } else {
        std::cout << "use existing webusb_context" << std::endl;
        std::cout << "webusb thread: " << _ctx->worker << std::endl;
    }

    *ctx = (libusb_context *)_ctx;
    return 0;
}

void libusb_exit(libusb_context *ctx) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
}

static ssize_t _dev_list_len = 0;
static libusb_device** _dev_list = NULL;

ssize_t libusb_get_device_list(libusb_context *ctx, libusb_device ***list) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif

    if(_dev_list) {
        *list = _dev_list;
        return _dev_list_len;
    }

    libusb_device** _list;

    size_t n = emscripten_dispatch_to_thread_sync(wc(ctx)->worker, EM_FUNC_SIG_III, _libusb_get_device_list, nullptr,
            1, &_list);

    libusb_device** l = (libusb_device**)malloc(sizeof(libusb_device*) * (n + 2));

    for (int i = 0; i < n; i++) {
        auto dc = (device_context*)malloc(sizeof(device_context));
        dc->ctx = wc(ctx);
        dc->idev = _list[i];
        l[i] = (libusb_device*)dc;
    }

    free(_list);

    l[n] = nullptr;
    l[n+1] = (libusb_device*)ctx;

    _dev_list = l;
    _dev_list_len = n;

    *list = l;

    return n;
}

void libusb_free_device_list(libusb_device **list, int unref_devices) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return;
    pthread_t thread;

    int n = 0;
    for (int i = 0; i < 100; i++) {
        if (list[i] == nullptr) {
            thread = ((webusb_context*)list[i+1])->worker;
            break;
        }
        n += 1;
    }

    libusb_device** _list = (libusb_device**)malloc(sizeof(libusb_device*) * n + 1);

    for (int i = 0; i < n; i++) {
        _list[i] = dc(list[i])->idev;
    }

    _list[n] = nullptr;

    free(list);

    emscripten_dispatch_to_thread_sync(thread, EM_FUNC_SIG_VII, _libusb_free_device_list, nullptr,
            _list, unref_devices);
}

int libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    std::cout << "webusb thread: " << _ctx->worker << std::endl;
    return emscripten_dispatch_to_thread_sync(wc(dev)->worker, EM_FUNC_SIG_III, _libusb_get_device_descriptor, nullptr,
            dc(dev)->idev, desc);
}

int LIBUSB_CALL libusb_open(libusb_device *dev, libusb_device_handle **dev_handle) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    *dev_handle = (libusb_device_handle*)wc(dev);
    return emscripten_dispatch_to_thread_sync(wc(dev)->worker, EM_FUNC_SIG_III, _libusb_open, nullptr,
            dc(dev)->idev, nullptr);
}

void LIBUSB_CALL libusb_close(libusb_device_handle *dev_handle) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_VI, _libusb_close, nullptr,
            nullptr);
}

int LIBUSB_CALL libusb_get_string_descriptor_ascii(libusb_device_handle *dev_handle,
    uint8_t desc_index, unsigned char *data, int length) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_IIIII, _libusb_get_string_descriptor_ascii, nullptr,
            nullptr, desc_index, data, length);
}

int LIBUSB_CALL libusb_set_configuration(libusb_device_handle *dev_handle, int configuration) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_III, _libusb_set_configuration, nullptr,
            nullptr, configuration);
}

int LIBUSB_CALL libusb_claim_interface(libusb_device_handle *dev_handle, int interface_number) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_III, _libusb_claim_interface, nullptr,
            nullptr, interface_number);
}

int LIBUSB_CALL libusb_release_interface(libusb_device_handle *dev_handle, int interface_number) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_III, _libusb_release_interface, nullptr,
            nullptr, interface_number);
}

int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *dev_handle,
    uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
    unsigned char *data, uint16_t wLength, unsigned int timeout) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_IIIIIIIII, _libusb_control_transfer, nullptr,
            nullptr, request_type, bRequest, wValue, wIndex, data, wLength, timeout);
}

int LIBUSB_CALL libusb_clear_halt(libusb_device_handle *dev_handle, unsigned char endpoint) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_III, _libusb_clear_halt, nullptr,
            nullptr, endpoint);
}

int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *transfer) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(transfer->dev_handle)->worker, EM_FUNC_SIG_II, _libusb_submit_transfer, nullptr,
            transfer);
}

int LIBUSB_CALL libusb_reset_device(libusb_device_handle *dev_handle) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_II, _libusb_reset_device, nullptr,
            nullptr);
}

int LIBUSB_CALL libusb_kernel_driver_active(libusb_device_handle *dev_handle, int interface_number) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_III, _libusb_kernel_driver_active, nullptr,
            nullptr, interface_number);
}

int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *transfer) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(transfer->dev_handle)->worker, EM_FUNC_SIG_II, _libusb_cancel_transfer, nullptr,
            transfer);
}

void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *transfer) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    emscripten_dispatch_to_thread_sync(wc(transfer->dev_handle)->worker, EM_FUNC_SIG_VI, _libusb_free_transfer, nullptr,
            transfer);
}

int LIBUSB_CALL libusb_handle_events_timeout(libusb_context *ctx, struct timeval *tv) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(ctx)->worker, EM_FUNC_SIG_III, _libusb_handle_events_timeout, nullptr,
            ctx, tv);
}

int LIBUSB_CALL libusb_handle_events_timeout_completed(libusb_context *ctx,
	struct timeval *tv, int *completed) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(_ctx->worker, EM_FUNC_SIG_IIII, _libusb_handle_events_timeout_completed, nullptr,
            nullptr, tv, completed);
}

int LIBUSB_CALL libusb_set_interface_alt_setting(libusb_device_handle *dev_handle,
	int interface_number, int alternate_setting) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return emscripten_dispatch_to_thread_sync(wc(dev_handle)->worker, EM_FUNC_SIG_IIII, _libusb_set_interface_alt_setting, nullptr,
            nullptr, interface_number, alternate_setting);
}

//
// Not Proxied
//

const struct libusb_version* libusb_get_version(void) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return _libusb_get_version();
};

struct libusb_transfer * LIBUSB_CALL libusb_alloc_transfer(int iso_packets) {
#ifdef DEBUG_TRACE
    std::cout << "> " << __func__ << std::endl;
#endif
    return _libusb_alloc_transfer(iso_packets);
}

static void LIBUSB_CALL sync_transfer_cb(struct libusb_transfer *transfer)
{
	int *completed = (int*) transfer->user_data;
	*completed = 1;
	/* caller interprets result and frees transfer */
}

static void sync_transfer_wait_for_completion(struct libusb_transfer *transfer)
{
	int r;
    int *completed = (int*)transfer->user_data;

	while (!*completed) {
		r = libusb_handle_events_completed(NULL, completed);
		if (r < 0) {
			if (r == LIBUSB_ERROR_INTERRUPTED)
				continue;
            std::cout << "libusb_handle_events completed failed, cancelling transfer and retrying: " << std::endl;
			libusb_cancel_transfer(transfer);
			continue;
		}
		if (NULL == transfer->dev_handle) {
			/* transfer completion after libusb_close() */
			transfer->status = LIBUSB_TRANSFER_NO_DEVICE;
			*completed = 1;
		}
	}
}

static int do_sync_bulk_transfer(struct libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *buffer, int length,
	int *transferred, unsigned int timeout, unsigned char type)
{
	struct libusb_transfer *transfer;
	int completed = 0;
	int r;

	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
        std::cout << "bulk transfer: no transfer allocated" << std::endl;
		return LIBUSB_ERROR_NO_MEM;
    }

	libusb_fill_bulk_transfer(transfer, dev_handle, endpoint, buffer, length,
		sync_transfer_cb, &completed, timeout);
	transfer->type = type;

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
        std::cout << "bulk transfer: failed to submit transfer" << std::endl;
		libusb_free_transfer(transfer);
		return r;
	}

	sync_transfer_wait_for_completion(transfer);

	if (transferred)
		*transferred = transfer->actual_length;

	switch (transfer->status) {
	case LIBUSB_TRANSFER_COMPLETED:
        std::cout << "bulk transfer: completed" << std::endl;
		r = 0;
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
        std::cout << "bulk transfer: timeout" << std::endl;
		r = LIBUSB_ERROR_TIMEOUT;
		break;
	case LIBUSB_TRANSFER_STALL:
        std::cout << "bulk transfer: stall" << std::endl;
		r = LIBUSB_ERROR_PIPE;
		break;
	case LIBUSB_TRANSFER_OVERFLOW:
        std::cout << "bulk transfer: overflow" << std::endl;
		r = LIBUSB_ERROR_OVERFLOW;
		break;
	case LIBUSB_TRANSFER_NO_DEVICE:
        std::cout << "bulk transfer: no device" << std::endl;
		r = LIBUSB_ERROR_NO_DEVICE;
		break;
	case LIBUSB_TRANSFER_ERROR:
	case LIBUSB_TRANSFER_CANCELLED:
        std::cout << "bulk transfer: no error" << std::endl;
		r = LIBUSB_ERROR_IO;
		break;
	default:
        std::cout << "bulk transfer: unrecognised status code " << transfer->status << std::endl;
		r = LIBUSB_ERROR_OTHER;
	}


	libusb_free_transfer(transfer);
	return r;
}

int LIBUSB_CALL libusb_handle_events_completed(
        libusb_context *ctx, int *completed) {
    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    return libusb_handle_events_timeout_completed(ctx, &tv, completed);
}

int LIBUSB_CALL libusb_bulk_transfer(libusb_device_handle *dev_handle,
    unsigned char endpoint, unsigned char *data, int length,
                                     int *actual_length, unsigned int timeout) {
    return do_sync_bulk_transfer(dev_handle, endpoint, data, length,
            actual_length, timeout, LIBUSB_TRANSFER_TYPE_BULK);
}

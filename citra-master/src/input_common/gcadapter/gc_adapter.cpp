// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <thread>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200) // nonstandard extension used : zero-sized array in struct/union
#endif
#include <libusb.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "common/logging/log.h"
#include "common/param_package.h"
#include "input_common/gcadapter/gc_adapter.h"

// Workaround for older libusb versions not having libusb_init_context.
// libusb_init is deprecated and causes a compile error in newer versions.
#if !defined(LIBUSB_API_VERSION) || (LIBUSB_API_VERSION < 0x0100010A)
#define libusb_init_context(a, b, c) libusb_init(a)
#endif

namespace GCAdapter {

Adapter::Adapter() {
    if (usb_adapter_handle != nullptr) {
        return;
    }
    const int init_res = libusb_init_context(&libusb_ctx, nullptr, 0);
    if (init_res == LIBUSB_SUCCESS) {
        adapter_scan_thread = std::thread(&Adapter::AdapterScanThread, this);
    } else {
        LOG_ERROR(Input, "libusb could not be initialized. failed with error = {}", init_res);
    }
}

Adapter::~Adapter() {
    JoinThreads();
    ClearLibusbHandle();
    ResetDevices();

    if (libusb_ctx) {
        libusb_exit(libusb_ctx);
    }
}

void Adapter::AdapterInputThread() {
    LOG_DEBUG(Input, "GC Adapter input thread started");
    s32 payload_size{};
    AdapterPayload adapter_payload{};

    if (adapter_scan_thread.joinable()) {
        adapter_scan_thread.join();
    }

    while (adapter_input_thread_running) {
        libusb_interrupt_transfer(usb_adapter_handle, input_endpoint, adapter_payload.data(),
                                  static_cast<s32>(adapter_payload.size()), &payload_size, 16);
        if (IsPayloadCorrect(adapter_payload, payload_size)) {
            UpdateControllers(adapter_payload);
        }
        std::this_thread::yield();
    }

    if (restart_scan_thread) {
        adapter_scan_thread = std::thread(&Adapter::AdapterScanThread, this);
        restart_scan_thread = false;
    }
}

bool Adapter::IsPayloadCorrect(const AdapterPayload& adapter_payload, s32 payload_size) {
    if (payload_size != static_cast<s32>(adapter_payload.size()) ||
        adapter_payload[0] != LIBUSB_DT_HID) {
        LOG_DEBUG(Input, "Error reading payload (size: {}, type: {:02x})", payload_size,
                  adapter_payload[0]);
        if (++input_error_counter > 20) {
            LOG_ERROR(Input, "GC adapter timeout, Is the adapter connected?");
            adapter_input_thread_running = false;
            restart_scan_thread = true;
        }
        return false;
    }

    input_error_counter = 0;
    return true;
}

void Adapter::UpdateControllers(const AdapterPayload& adapter_payload) {
    for (std::size_t port = 0; port < pads.size(); ++port) {
        const std::size_t offset = 1 + (9 * port);
        const auto type = static_cast<ControllerTypes>(adapter_payload[offset] >> 4);
        UpdatePadType(port, type);
        if (DeviceConnected(port)) {
            const u8 b1 = adapter_payload[offset + 1];
            const u8 b2 = adapter_payload[offset + 2];
            UpdateStateButtons(port, b1, b2);
            UpdateStateAxes(port, adapter_payload);
            if (configuring) {
                UpdateSettings(port);
            }
        }
    }
}

void Adapter::UpdatePadType(std::size_t port, ControllerTypes pad_type) {
    if (pads[port].type == pad_type) {
        return;
    }
    // Device changed reset device and set new type
    ResetDevice(port);
    pads[port].type = pad_type;
}

void Adapter::UpdateStateButtons(std::size_t port, u8 b1, u8 b2) {
    if (port >= pads.size()) {
        return;
    }

    static constexpr std::array<PadButton, 8> b1_buttons{
        PadButton::ButtonA,    PadButton::ButtonB,     PadButton::ButtonX,    PadButton::ButtonY,
        PadButton::ButtonLeft, PadButton::ButtonRight, PadButton::ButtonDown, PadButton::ButtonUp,
    };

    static constexpr std::array<PadButton, 4> b2_buttons{
        PadButton::ButtonStart,
        PadButton::TriggerZ,
        PadButton::TriggerR,
        PadButton::TriggerL,
    };
    pads[port].buttons = 0;
    for (std::size_t i = 0; i < b1_buttons.size(); ++i) {
        if ((b1 & (1U << i)) != 0) {
            pads[port].buttons =
                static_cast<u16>(pads[port].buttons | static_cast<u16>(b1_buttons[i]));
            pads[port].last_button = b1_buttons[i];
        }
    }

    for (std::size_t j = 0; j < b2_buttons.size(); ++j) {
        if ((b2 & (1U << j)) != 0) {
            pads[port].buttons =
                static_cast<u16>(pads[port].buttons | static_cast<u16>(b2_buttons[j]));
            pads[port].last_button = b2_buttons[j];
        }
    }
}

void Adapter::UpdateStateAxes(std::size_t port, const AdapterPayload& adapter_payload) {
    if (port >= pads.size()) {
        return;
    }

    const std::size_t offset = 1 + (9 * port);
    static constexpr std::array<PadAxes, 6> axes{
        PadAxes::StickX,    PadAxes::StickY,      PadAxes::SubstickX,
        PadAxes::SubstickY, PadAxes::TriggerLeft, PadAxes::TriggerRight,
    };

    for (const PadAxes axis : axes) {
        const auto index = static_cast<std::size_t>(axis);
        const u8 axis_value = adapter_payload[offset + 3 + index];
        if (pads[port].axis_origin[index] == 255) {
            pads[port].axis_origin[index] = axis_value;
        }
        pads[port].axis_values[index] =
            static_cast<s16>(axis_value - pads[port].axis_origin[index]);
    }
}

void Adapter::UpdateSettings(std::size_t port) {
    if (port >= pads.size()) {
        return;
    }

    constexpr u8 axis_threshold = 50;
    GCPadStatus pad_status = {port};

    if (pads[port].buttons != 0) {
        pad_status.button = pads[port].last_button;
        pad_queue.Push(pad_status);
    }

    // Accounting for a threshold here to ensure an intentional press
    for (std::size_t i = 0; i < pads[port].axis_values.size(); ++i) {
        const s16 value = pads[port].axis_values[i];

        if (value > axis_threshold || value < -axis_threshold) {
            pad_status.axis = static_cast<PadAxes>(i);
            pad_status.axis_value = value;
            pad_status.axis_threshold = axis_threshold;
            pad_queue.Push(pad_status);
        }
    }
}

void Adapter::AdapterScanThread() {
    adapter_scan_thread_running = true;
    adapter_input_thread_running = false;
    if (adapter_input_thread.joinable()) {
        adapter_input_thread.join();
    }
    ClearLibusbHandle();
    ResetDevices();
    while (adapter_scan_thread_running && !adapter_input_thread_running) {
        Setup();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Adapter::Setup() {
    usb_adapter_handle = libusb_open_device_with_vid_pid(libusb_ctx, 0x057e, 0x0337);

    if (usb_adapter_handle == NULL) {
        return;
    }
    if (!CheckDeviceAccess()) {
        ClearLibusbHandle();
        return;
    }

    libusb_device* device = libusb_get_device(usb_adapter_handle);

    LOG_INFO(Input, "GC adapter is now connected");
    // GC Adapter found and accessible, registering it
    if (GetGCEndpoint(device)) {
        adapter_scan_thread_running = false;
        adapter_input_thread_running = true;
        input_error_counter = 0;
        adapter_input_thread = std::thread(&Adapter::AdapterInputThread, this);
    }
}

bool Adapter::CheckDeviceAccess() {
    // This fixes payload problems from offbrand GCAdapters
    const s32 control_transfer_error =
        libusb_control_transfer(usb_adapter_handle, 0x21, 11, 0x0001, 0, nullptr, 0, 1000);
    if (control_transfer_error < 0) {
        LOG_ERROR(Input, "libusb_control_transfer failed with error= {}", control_transfer_error);
    }

    s32 kernel_driver_error = libusb_kernel_driver_active(usb_adapter_handle, 0);
    if (kernel_driver_error == 1) {
        kernel_driver_error = libusb_detach_kernel_driver(usb_adapter_handle, 0);
        if (kernel_driver_error != 0 && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
            LOG_ERROR(Input, "libusb_detach_kernel_driver failed with error = {}",
                      kernel_driver_error);
        }
    }

    if (kernel_driver_error && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    const int interface_claim_error = libusb_claim_interface(usb_adapter_handle, 0);
    if (interface_claim_error) {
        LOG_ERROR(Input, "libusb_claim_interface failed with error = {}", interface_claim_error);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    return true;
}

bool Adapter::GetGCEndpoint(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    const int config_descriptor_return = libusb_get_config_descriptor(device, 0, &config);
    if (config_descriptor_return != LIBUSB_SUCCESS) {
        LOG_ERROR(Input, "libusb_get_config_descriptor failed with error = {}",
                  config_descriptor_return);
        return false;
    }

    for (u8 ic = 0; ic < config->bNumInterfaces; ic++) {
        const libusb_interface* interfaceContainer = &config->interface[ic];
        for (int i = 0; i < interfaceContainer->num_altsetting; i++) {
            const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
            for (u8 e = 0; e < interface->bNumEndpoints; e++) {
                const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
                if ((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) != 0) {
                    input_endpoint = endpoint->bEndpointAddress;
                } else {
                    output_endpoint = endpoint->bEndpointAddress;
                }
            }
        }
    }
    // This transfer seems to be responsible for clearing the state of the adapter
    // Used to clear the "busy" state of when the device is unexpectedly unplugged
    unsigned char clear_payload = 0x13;
    libusb_interrupt_transfer(usb_adapter_handle, output_endpoint, &clear_payload,
                              sizeof(clear_payload), nullptr, 16);
    return true;
}

void Adapter::JoinThreads() {
    restart_scan_thread = false;
    adapter_input_thread_running = false;
    adapter_scan_thread_running = false;

    if (adapter_scan_thread.joinable()) {
        adapter_scan_thread.join();
    }

    if (adapter_input_thread.joinable()) {
        adapter_input_thread.join();
    }
}

void Adapter::ClearLibusbHandle() {
    if (usb_adapter_handle) {
        libusb_release_interface(usb_adapter_handle, 1);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
    }
}

void Adapter::ResetDevices() {
    for (std::size_t i = 0; i < pads.size(); ++i) {
        ResetDevice(i);
    }
}

void Adapter::ResetDevice(std::size_t port) {
    pads[port].type = ControllerTypes::None;
    pads[port].buttons = 0;
    pads[port].last_button = PadButton::Undefined;
    pads[port].axis_values.fill(0);
    pads[port].axis_origin.fill(255);
}

std::vector<Common::ParamPackage> Adapter::GetInputDevices() const {
    std::vector<Common::ParamPackage> devices;
    for (std::size_t port = 0; port < pads.size(); ++port) {
        if (!DeviceConnected(port)) {
            continue;
        }
        std::string name = fmt::format("Gamecube Controller {}", port + 1);
        devices.emplace_back(Common::ParamPackage{
            {"class", "gcpad"},
            {"display", std::move(name)},
            {"port", std::to_string(port)},
        });
    }
    return devices;
}

bool Adapter::DeviceConnected(std::size_t port) const {
    return pads[port].type != ControllerTypes::None;
}

void Adapter::BeginConfiguration() {
    pad_queue.Clear();
    configuring = true;
}

void Adapter::EndConfiguration() {
    pad_queue.Clear();
    configuring = false;
}

Common::SPSCQueue<GCPadStatus>& Adapter::GetPadQueue() {
    return pad_queue;
}

const Common::SPSCQueue<GCPadStatus>& Adapter::GetPadQueue() const {
    return pad_queue;
}

GCController& Adapter::GetPadState(std::size_t port) {
    return pads.at(port);
}

const GCController& Adapter::GetPadState(std::size_t port) const {
    return pads.at(port);
}

} // namespace GCAdapter

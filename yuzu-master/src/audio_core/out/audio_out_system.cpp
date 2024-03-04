// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mutex>

#include "audio_core/audio_event.h"
#include "audio_core/audio_manager.h"
#include "audio_core/out/audio_out_system.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_event.h"

namespace AudioCore::AudioOut {

System::System(Core::System& system_, Kernel::KEvent* event_, size_t session_id_)
    : system{system_}, buffer_event{event_},
      session_id{session_id_}, session{std::make_unique<DeviceSession>(system_)} {}

System::~System() {
    Finalize();
}

void System::Finalize() {
    Stop();
    session->Finalize();
}

std::string_view System::GetDefaultOutputDeviceName() const {
    return "DeviceOut";
}

Result System::IsConfigValid(std::string_view device_name,
                             const AudioOutParameter& in_params) const {
    if ((device_name.size() > 0) && (device_name != GetDefaultOutputDeviceName())) {
        return Service::Audio::ResultNotFound;
    }

    if (in_params.sample_rate != TargetSampleRate && in_params.sample_rate > 0) {
        return Service::Audio::ResultInvalidSampleRate;
    }

    if (in_params.channel_count == 0 || in_params.channel_count == 2 ||
        in_params.channel_count == 6) {
        return ResultSuccess;
    }

    return Service::Audio::ResultInvalidChannelCount;
}

Result System::Initialize(std::string device_name, const AudioOutParameter& in_params,
                          Kernel::KProcess* handle_, u64 applet_resource_user_id_) {
    auto result = IsConfigValid(device_name, in_params);
    if (result.IsError()) {
        return result;
    }

    handle = handle_;
    applet_resource_user_id = applet_resource_user_id_;
    if (device_name.empty() || device_name[0] == '\0') {
        name = std::string(GetDefaultOutputDeviceName());
    } else {
        name = std::move(device_name);
    }

    sample_rate = TargetSampleRate;
    sample_format = SampleFormat::PcmInt16;
    channel_count = in_params.channel_count <= 2 ? 2 : 6;
    volume = 1.0f;
    return ResultSuccess;
}

void System::StartSession() {
    session->Start();
}

size_t System::GetSessionId() const {
    return session_id;
}

Result System::Start() {
    if (state != State::Stopped) {
        return Service::Audio::ResultOperationFailed;
    }

    session->Initialize(name, sample_format, channel_count, session_id, handle,
                        applet_resource_user_id, Sink::StreamType::Out);
    session->SetVolume(volume);
    session->Start();
    state = State::Started;

    boost::container::static_vector<AudioBuffer, BufferCount> buffers_to_flush{};
    buffers.RegisterBuffers(buffers_to_flush);
    session->AppendBuffers(buffers_to_flush);
    session->SetRingSize(static_cast<u32>(buffers_to_flush.size()));

    return ResultSuccess;
}

Result System::Stop() {
    if (state == State::Started) {
        session->Stop();
        session->SetVolume(0.0f);
        session->ClearBuffers();
        if (buffers.ReleaseBuffers(system.CoreTiming(), *session, true)) {
            buffer_event->Signal();
        }
        state = State::Stopped;
    }

    return ResultSuccess;
}

bool System::AppendBuffer(const AudioOutBuffer& buffer, u64 tag) {
    if (buffers.GetTotalBufferCount() == BufferCount) {
        return false;
    }

    const auto timestamp{buffers.GetNextTimestamp()};
    const AudioBuffer new_buffer{
        .start_timestamp = timestamp,
        .end_timestamp = timestamp + buffer.size / (channel_count * sizeof(s16)),
        .played_timestamp = 0,
        .samples = buffer.samples,
        .tag = tag,
        .size = buffer.size,
    };

    buffers.AppendBuffer(new_buffer);
    RegisterBuffers();

    return true;
}

void System::RegisterBuffers() {
    if (state == State::Started) {
        boost::container::static_vector<AudioBuffer, BufferCount> registered_buffers{};
        buffers.RegisterBuffers(registered_buffers);
        session->AppendBuffers(registered_buffers);
    }
}

void System::ReleaseBuffers() {
    bool signal{buffers.ReleaseBuffers(system.CoreTiming(), *session, false)};
    if (signal) {
        // Signal if any buffer was released, or if none are registered, we need more.
        buffer_event->Signal();
    }
}

u32 System::GetReleasedBuffers(std::span<u64> tags) {
    return buffers.GetReleasedBuffers(tags);
}

bool System::FlushAudioOutBuffers() {
    if (state != State::Started) {
        return false;
    }

    u32 buffers_released{};
    buffers.FlushBuffers(buffers_released);

    if (buffers_released > 0) {
        buffer_event->Signal();
    }
    return true;
}

u16 System::GetChannelCount() const {
    return channel_count;
}

u32 System::GetSampleRate() const {
    return sample_rate;
}

SampleFormat System::GetSampleFormat() const {
    return sample_format;
}

State System::GetState() {
    switch (state) {
    case State::Started:
    case State::Stopped:
        return state;
    default:
        LOG_ERROR(Service_Audio, "AudioOut invalid state!");
        state = State::Stopped;
        break;
    }
    return state;
}

std::string System::GetName() const {
    return name;
}

f32 System::GetVolume() const {
    return volume;
}

void System::SetVolume(const f32 volume_) {
    volume = volume_;
    session->SetVolume(volume_);
}

bool System::ContainsAudioBuffer(const u64 tag) const {
    return buffers.ContainsBuffer(tag);
}

u32 System::GetBufferCount() const {
    return buffers.GetAppendedRegisteredCount();
}

u64 System::GetPlayedSampleCount() const {
    return session->GetPlayedSampleCount();
}

} // namespace AudioCore::AudioOut

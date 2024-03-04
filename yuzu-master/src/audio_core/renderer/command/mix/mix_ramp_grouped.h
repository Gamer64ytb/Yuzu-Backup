// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>

#include "audio_core/renderer/command/icommand.h"
#include "common/common_types.h"

namespace AudioCore::ADSP::AudioRenderer {
class CommandListProcessor;
}

namespace AudioCore::Renderer {

/**
 * AudioRenderer command for mixing multiple input mix buffers to multiple output mix buffers, with
 * a volume applied to the input, and volume ramping to smooth out the transition.
 */
struct MixRampGroupedCommand : ICommand {
    /**
     * Print this command's information to a string.
     *
     * @param processor - The CommandListProcessor processing this command.
     * @param string    - The string to print into.
     */
    void Dump(const AudioRenderer::CommandListProcessor& processor, std::string& string) override;

    /**
     * Process this command.
     *
     * @param processor - The CommandListProcessor processing this command.
     */
    void Process(const AudioRenderer::CommandListProcessor& processor) override;

    /**
     * Verify this command's data is valid.
     *
     * @param processor - The CommandListProcessor processing this command.
     * @return True if the command is valid, otherwise false.
     */
    bool Verify(const AudioRenderer::CommandListProcessor& processor) override;

    /// Fixed point precision
    u8 precision;
    /// Number of mix buffers to mix
    u32 buffer_count;
    /// Input mix buffer indexes for each mix buffer
    std::array<s16, MaxMixBuffers> inputs;
    /// Output mix buffer indexes for each mix buffer
    std::array<s16, MaxMixBuffers> outputs;
    /// Previous mix volumes for each mix buffer
    std::array<f32, MaxMixBuffers> prev_volumes;
    /// Current mix volumes for each mix buffer
    std::array<f32, MaxMixBuffers> volumes;
    /// Pointer to the previous sample buffer, used for depop
    CpuAddr previous_samples;
};

} // namespace AudioCore::Renderer

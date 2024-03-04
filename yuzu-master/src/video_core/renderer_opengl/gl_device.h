// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string>

#include "common/common_types.h"
#include "core/frontend/emu_window.h"
#include "shader_recompiler/stage.h"

namespace Settings {
enum class ShaderBackend : u32;
};

namespace OpenGL {

class Device {
public:
    explicit Device(Core::Frontend::EmuWindow& emu_window);

    [[nodiscard]] std::string GetVendorName() const;

    u64 GetCurrentDedicatedVideoMemory() const;

    u32 GetMaxUniformBuffers(Shader::Stage stage) const noexcept {
        return max_uniform_buffers[static_cast<size_t>(stage)];
    }

    size_t GetUniformBufferAlignment() const {
        return uniform_buffer_alignment;
    }

    size_t GetShaderStorageBufferAlignment() const {
        return shader_storage_alignment;
    }

    u32 GetMaxVertexAttributes() const {
        return max_vertex_attributes;
    }

    u32 GetMaxVaryings() const {
        return max_varyings;
    }

    u32 GetMaxComputeSharedMemorySize() const {
        return max_compute_shared_memory_size;
    }

    u32 GetMaxGLASMStorageBufferBlocks() const {
        return max_glasm_storage_buffer_blocks;
    }

    bool HasWarpIntrinsics() const {
        return has_warp_intrinsics;
    }

    bool HasShaderBallot() const {
        return has_shader_ballot;
    }

    bool HasVertexViewportLayer() const {
        return has_vertex_viewport_layer;
    }

    bool HasImageLoadFormatted() const {
        return has_image_load_formatted;
    }

    bool HasTextureShadowLod() const {
        return has_texture_shadow_lod;
    }

    bool HasVertexBufferUnifiedMemory() const {
        return has_vertex_buffer_unified_memory;
    }

    bool HasASTC() const {
        return has_astc;
    }

    bool HasVariableAoffi() const {
        return has_variable_aoffi;
    }

    bool HasComponentIndexingBug() const {
        return has_component_indexing_bug;
    }

    bool HasPreciseBug() const {
        return has_precise_bug;
    }

    bool HasBrokenTextureViewFormats() const {
        return has_broken_texture_view_formats;
    }

    bool HasFastBufferSubData() const {
        return has_fast_buffer_sub_data;
    }

    bool HasNvViewportArray2() const {
        return has_nv_viewport_array2;
    }

    bool HasDerivativeControl() const {
        return has_derivative_control;
    }

    bool HasDebuggingToolAttached() const {
        return has_debugging_tool_attached;
    }

    bool UseAssemblyShaders() const {
        return use_assembly_shaders;
    }

    bool UseAsynchronousShaders() const {
        return use_asynchronous_shaders;
    }

    bool UseDriverCache() const {
        return use_driver_cache;
    }

    bool HasDepthBufferFloat() const {
        return has_depth_buffer_float;
    }

    bool HasGeometryShaderPassthrough() const {
        return has_geometry_shader_passthrough;
    }

    bool HasNvGpuShader5() const {
        return has_nv_gpu_shader_5;
    }

    bool HasShaderInt64() const {
        return has_shader_int64;
    }

    bool HasAmdShaderHalfFloat() const {
        return has_amd_shader_half_float;
    }

    bool HasSparseTexture2() const {
        return has_sparse_texture_2;
    }

    bool HasDrawTexture() const {
        return has_draw_texture;
    }

    bool IsWarpSizePotentiallyLargerThanGuest() const {
        return warp_size_potentially_larger_than_guest;
    }

    bool NeedsFastmathOff() const {
        return need_fastmath_off;
    }

    bool HasCbufFtouBug() const {
        return has_cbuf_ftou_bug;
    }

    bool HasBoolRefBug() const {
        return has_bool_ref_bug;
    }

    Settings::ShaderBackend GetShaderBackend() const {
        return shader_backend;
    }

    bool IsAmd() const {
        return vendor_name == "ATI Technologies Inc.";
    }

    bool IsIntel() const {
        return vendor_name == "Intel";
    }

    bool CanReportMemoryUsage() const {
        return can_report_memory;
    }

    bool StrictContextRequired() const {
        return strict_context_required;
    }

    bool SupportsConditionalBarriers() const {
        return supports_conditional_barriers;
    }

    bool HasLmemPerfBug() const {
        return has_lmem_perf_bug;
    }

private:
    static bool TestVariableAoffi();
    static bool TestPreciseBug();

    std::array<u32, Shader::MaxStageTypes> max_uniform_buffers{};
    size_t uniform_buffer_alignment{};
    size_t shader_storage_alignment{};
    u32 max_vertex_attributes{};
    u32 max_varyings{};
    u32 max_compute_shared_memory_size{};
    u32 max_glasm_storage_buffer_blocks{};

    Settings::ShaderBackend shader_backend{};

    bool has_warp_intrinsics{};
    bool has_shader_ballot{};
    bool has_vertex_viewport_layer{};
    bool has_image_load_formatted{};
    bool has_texture_shadow_lod{};
    bool has_vertex_buffer_unified_memory{};
    bool has_astc{};
    bool has_variable_aoffi{};
    bool has_component_indexing_bug{};
    bool has_precise_bug{};
    bool has_broken_texture_view_formats{};
    bool has_fast_buffer_sub_data{};
    bool has_nv_viewport_array2{};
    bool has_derivative_control{};
    bool has_debugging_tool_attached{};
    bool use_assembly_shaders{};
    bool use_asynchronous_shaders{};
    bool use_driver_cache{};
    bool has_depth_buffer_float{};
    bool has_geometry_shader_passthrough{};
    bool has_nv_gpu_shader_5{};
    bool has_shader_int64{};
    bool has_amd_shader_half_float{};
    bool has_sparse_texture_2{};
    bool has_draw_texture{};
    bool warp_size_potentially_larger_than_guest{};
    bool need_fastmath_off{};
    bool has_cbuf_ftou_bug{};
    bool has_bool_ref_bug{};
    bool can_report_memory{};
    bool strict_context_required{};
    bool supports_conditional_barriers{};
    bool has_lmem_perf_bug{};

    std::string vendor_name;
};

} // namespace OpenGL

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#include "common/literals.h"
#include "common/logging/log.h"
#include "common/polyfill_ranges.h"
#include "common/settings.h"
#include "shader_recompiler/stage.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

using namespace Common::Literals;

namespace OpenGL {
namespace {
constexpr std::array LIMIT_UBOS = {
    GL_MAX_VERTEX_UNIFORM_BLOCKS,          GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS,
    GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS, GL_MAX_GEOMETRY_UNIFORM_BLOCKS,
    GL_MAX_FRAGMENT_UNIFORM_BLOCKS,        GL_MAX_COMPUTE_UNIFORM_BLOCKS,
};

template <typename T>
T GetInteger(GLenum pname) {
    GLint temporary;
    glGetIntegerv(pname, &temporary);
    return static_cast<T>(temporary);
}

bool TestProgram(const GLchar* glsl) {
    const GLuint shader{glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &glsl)};
    GLint link_status;
    glGetProgramiv(shader, GL_LINK_STATUS, &link_status);
    glDeleteProgram(shader);
    return link_status == GL_TRUE;
}

std::vector<std::string_view> GetExtensions() {
    GLint num_extensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    std::vector<std::string_view> extensions;
    extensions.reserve(num_extensions);
    for (GLint index = 0; index < num_extensions; ++index) {
        extensions.push_back(
            reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(index))));
    }
    return extensions;
}

bool HasExtension(std::span<const std::string_view> extensions, std::string_view extension) {
    return std::ranges::find(extensions, extension) != extensions.end();
}

std::array<u32, Shader::MaxStageTypes> BuildMaxUniformBuffers() noexcept {
    std::array<u32, Shader::MaxStageTypes> max;
    std::ranges::transform(LIMIT_UBOS, max.begin(), &GetInteger<u32>);
    return max;
}

bool IsASTCSupported() {
    static constexpr std::array targets{
        GL_TEXTURE_2D,
        GL_TEXTURE_2D_ARRAY,
    };
    static constexpr std::array formats{
        GL_COMPRESSED_RGBA_ASTC_4x4_KHR,           GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
        GL_COMPRESSED_RGBA_ASTC_5x5_KHR,           GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
        GL_COMPRESSED_RGBA_ASTC_6x6_KHR,           GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
        GL_COMPRESSED_RGBA_ASTC_8x6_KHR,           GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
        GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
        GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
        GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,  GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,  GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
        GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR,
    };
    static constexpr std::array required_support{
        GL_VERTEX_TEXTURE,   GL_TESS_CONTROL_TEXTURE, GL_TESS_EVALUATION_TEXTURE,
        GL_GEOMETRY_TEXTURE, GL_FRAGMENT_TEXTURE,     GL_COMPUTE_TEXTURE,
    };
    for (const GLenum target : targets) {
        for (const GLenum format : formats) {
            for (const GLenum support : required_support) {
                GLint value;
                glGetInternalformativ(target, format, support, 1, &value);
                if (value != GL_FULL_SUPPORT) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool HasSlowSoftwareAstc(std::string_view vendor_name, std::string_view renderer) {
// ifdef for Unix reduces string comparisons for non-Windows drivers, and Intel
#ifdef YUZU_UNIX
    // Sorted vaguely by how likely a vendor is to appear
    if (vendor_name == "AMD") {
        // RadeonSI
        return true;
    }
    if (vendor_name == "Intel") {
        // Must be inside YUZU_UNIX ifdef as the Windows driver uses the same vendor string
        // iris, crocus
        const bool is_intel_dg = (renderer.find("DG") != std::string_view::npos);
        return is_intel_dg;
    }
    if (vendor_name == "nouveau") {
        return true;
    }
    if (vendor_name == "X.Org") {
        // R600
        return true;
    }
#endif
    if (vendor_name == "Collabora Ltd") {
        // Zink
        return true;
    }
    if (vendor_name == "Microsoft Corporation") {
        // d3d12
        return true;
    }
    if (vendor_name == "Mesa/X.org") {
        // llvmpipe, softpipe, virgl
        return true;
    }
    return false;
}

[[nodiscard]] bool IsDebugToolAttached(std::span<const std::string_view> extensions) {
    const bool nsight = std::getenv("NVTX_INJECTION64_PATH") || std::getenv("NSIGHT_LAUNCHED");
    return nsight || HasExtension(extensions, "GL_EXT_debug_tool") ||
           Settings::values.renderer_debug.GetValue();
}
} // Anonymous namespace

Device::Device(Core::Frontend::EmuWindow& emu_window) {
    if (!GLAD_GL_VERSION_4_6) {
        LOG_ERROR(Render_OpenGL, "OpenGL 4.6 is not available");
        throw std::runtime_error{"Insufficient version"};
    }
    vendor_name = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const std::string_view version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const std::string_view renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const std::vector extensions = GetExtensions();

    const bool is_nvidia = vendor_name == "NVIDIA Corporation";
    const bool is_amd = vendor_name == "ATI Technologies Inc.";
    const bool is_intel = vendor_name == "Intel";

    const bool has_slow_software_astc =
        !is_nvidia && !is_amd && HasSlowSoftwareAstc(vendor_name, renderer);

#ifdef __unix__
    constexpr bool is_linux = true;
#else
    constexpr bool is_linux = false;
#endif

    bool disable_fast_buffer_sub_data = false;
    if (is_nvidia && version == "4.6.0 NVIDIA 443.24") {
        LOG_WARNING(
            Render_OpenGL,
            "Beta driver 443.24 is known to have issues. There might be performance issues.");
        disable_fast_buffer_sub_data = true;
    }
    max_uniform_buffers = BuildMaxUniformBuffers();
    uniform_buffer_alignment = GetInteger<size_t>(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
    shader_storage_alignment = GetInteger<size_t>(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT);
    max_vertex_attributes = GetInteger<u32>(GL_MAX_VERTEX_ATTRIBS);
    max_varyings = GetInteger<u32>(GL_MAX_VARYING_VECTORS);
    max_compute_shared_memory_size = GetInteger<u32>(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);
    max_glasm_storage_buffer_blocks = GetInteger<u32>(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS);
    has_warp_intrinsics = GLAD_GL_NV_gpu_shader5 && GLAD_GL_NV_shader_thread_group &&
                          GLAD_GL_NV_shader_thread_shuffle;
    has_shader_ballot = GLAD_GL_ARB_shader_ballot;
    has_vertex_viewport_layer = GLAD_GL_ARB_shader_viewport_layer_array;
    has_image_load_formatted = HasExtension(extensions, "GL_EXT_shader_image_load_formatted");
    has_texture_shadow_lod = HasExtension(extensions, "GL_EXT_texture_shadow_lod");
    has_astc = !has_slow_software_astc && IsASTCSupported();
    has_variable_aoffi = TestVariableAoffi();
    has_component_indexing_bug = false;
    has_precise_bug = TestPreciseBug();
    has_broken_texture_view_formats = (!is_linux && is_intel);
    has_nv_viewport_array2 = GLAD_GL_NV_viewport_array2;
    has_derivative_control = GLAD_GL_ARB_derivative_control;
    has_vertex_buffer_unified_memory = GLAD_GL_NV_vertex_buffer_unified_memory;
    has_debugging_tool_attached = IsDebugToolAttached(extensions);
    has_depth_buffer_float = HasExtension(extensions, "GL_NV_depth_buffer_float");
    has_geometry_shader_passthrough = GLAD_GL_NV_geometry_shader_passthrough;
    has_nv_gpu_shader_5 = GLAD_GL_NV_gpu_shader5;
    has_shader_int64 = HasExtension(extensions, "GL_ARB_gpu_shader_int64");
    has_amd_shader_half_float = GLAD_GL_AMD_gpu_shader_half_float;
    has_sparse_texture_2 = GLAD_GL_ARB_sparse_texture2;
    has_draw_texture = GLAD_GL_NV_draw_texture;
    warp_size_potentially_larger_than_guest = !is_nvidia && !is_intel;
    need_fastmath_off = is_nvidia;
    can_report_memory = GLAD_GL_NVX_gpu_memory_info;

    // At the moment of writing this, only Nvidia's driver optimizes BufferSubData on exclusive
    // uniform buffers as "push constants"
    has_fast_buffer_sub_data = is_nvidia && !disable_fast_buffer_sub_data;

    shader_backend = Settings::values.shader_backend.GetValue();
    use_assembly_shaders = shader_backend == Settings::ShaderBackend::Glasm &&
                           GLAD_GL_NV_gpu_program5 && GLAD_GL_NV_compute_program5 &&
                           GLAD_GL_NV_transform_feedback && GLAD_GL_NV_transform_feedback2;
    if (shader_backend == Settings::ShaderBackend::Glasm && !use_assembly_shaders) {
        LOG_ERROR(Render_OpenGL, "Assembly shaders enabled but not supported");
        shader_backend = Settings::ShaderBackend::Glsl;
    }

    if (shader_backend == Settings::ShaderBackend::Glsl && is_nvidia) {
        const std::string_view driver_version = version.substr(13);
        const int version_major =
            std::atoi(driver_version.substr(0, driver_version.find(".")).data());
        if (version_major >= 495) {
            has_cbuf_ftou_bug = true;
            has_bool_ref_bug = true;
        }
    }
    has_lmem_perf_bug = is_nvidia;

    strict_context_required = emu_window.StrictContextRequired();
    // Blocks Intel OpenGL drivers on Windows from using asynchronous shader compilation.
    // Blocks EGL on Wayland from using asynchronous shader compilation.
    const bool blacklist_async_shaders = (is_intel && !is_linux) || strict_context_required;
    use_asynchronous_shaders =
        Settings::values.use_asynchronous_shaders.GetValue() && !blacklist_async_shaders;
    use_driver_cache = is_nvidia;
    supports_conditional_barriers = !is_intel;

    LOG_INFO(Render_OpenGL, "Renderer_VariableAOFFI: {}", has_variable_aoffi);
    LOG_INFO(Render_OpenGL, "Renderer_ComponentIndexingBug: {}", has_component_indexing_bug);
    LOG_INFO(Render_OpenGL, "Renderer_PreciseBug: {}", has_precise_bug);
    LOG_INFO(Render_OpenGL, "Renderer_BrokenTextureViewFormats: {}",
             has_broken_texture_view_formats);
    if (Settings::values.use_asynchronous_shaders.GetValue() && !use_asynchronous_shaders) {
        LOG_WARNING(Render_OpenGL, "Asynchronous shader compilation enabled but not supported");
    }
}

std::string Device::GetVendorName() const {
    if (vendor_name == "NVIDIA Corporation") {
        return "NVIDIA";
    }
    if (vendor_name == "ATI Technologies Inc.") {
        return "AMD";
    }
    if (vendor_name == "Intel") {
        // For Mesa, `Intel` is an overloaded vendor string that could mean crocus or iris.
        // Simply return `INTEL` for those as well as the Windows driver.
        return "Intel";
    }
    if (vendor_name == "Intel Open Source Technology Center") {
        return "i965";
    }
    if (vendor_name == "Mesa Project") {
        return "i915";
    }
    if (vendor_name == "Mesa/X.org") {
        // This vendor string is overloaded between llvmpipe, softpipe, and virgl, so just return
        // MESA instead of one of those driver names.
        return "Mesa";
    }
    if (vendor_name == "AMD") {
        return "RadeonSI";
    }
    if (vendor_name == "nouveau") {
        return "Nouveau";
    }
    if (vendor_name == "X.Org") {
        return "R600";
    }
    if (vendor_name == "Collabora Ltd") {
        return "Zink";
    }
    if (vendor_name == "Intel Corporation") {
        return "OpenSWR";
    }
    if (vendor_name == "Microsoft Corporation") {
        return "D3D12";
    }
    if (vendor_name == "NVIDIA") {
        // Mesa's tegra driver reports `NVIDIA`. Only present in this list because the default
        // strategy would have returned `NVIDIA` here for this driver, the same result as the
        // proprietary driver.
        return "Tegra";
    }
    return vendor_name;
}

bool Device::TestVariableAoffi() {
    return TestProgram(R"(#version 430 core
// This is a unit test, please ignore me on apitrace bug reports.
uniform sampler2D tex;
uniform ivec2 variable_offset;
out vec4 output_attribute;
void main() {
    output_attribute = textureOffset(tex, vec2(0), variable_offset);
})");
}

bool Device::TestPreciseBug() {
    return !TestProgram(R"(#version 430 core
in vec3 coords;
out float out_value;
uniform sampler2DShadow tex;
void main() {
    precise float tmp_value = vec4(texture(tex, coords)).x;
    out_value = tmp_value;
})");
}

u64 Device::GetCurrentDedicatedVideoMemory() const {
    GLint cur_avail_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &cur_avail_mem_kb);
    return static_cast<u64>(cur_avail_mem_kb) * 1_KiB;
}

} // namespace OpenGL

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/thread_worker.h"
#include "shader_recompiler/backend/glasm/emit_glasm.h"
#include "shader_recompiler/backend/glsl/emit_glsl.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/profile.h"
#include "video_core/engines/draw_manager.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_environment.h"
#include "video_core/shader_notify.h"

namespace OpenGL {
namespace {
using Shader::Backend::GLASM::EmitGLASM;
using Shader::Backend::GLSL::EmitGLSL;
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::ConvertLegacyToGeneric;
using Shader::Maxwell::GenerateGeometryPassthrough;
using Shader::Maxwell::MergeDualVertexPrograms;
using Shader::Maxwell::TranslateProgram;
using VideoCommon::ComputeEnvironment;
using VideoCommon::FileEnvironment;
using VideoCommon::GenericEnvironment;
using VideoCommon::GraphicsEnvironment;
using VideoCommon::LoadPipelines;
using VideoCommon::SerializePipeline;
using Context = ShaderContext::Context;

constexpr u32 CACHE_VERSION = 10;

template <typename Container>
auto MakeSpan(Container& container) {
    return std::span(container.data(), container.size());
}

Shader::OutputTopology MaxwellToOutputTopology(Maxwell::PrimitiveTopology topology) {
    switch (topology) {
    case Maxwell::PrimitiveTopology::Points:
        return Shader::OutputTopology::PointList;
    case Maxwell::PrimitiveTopology::LineStrip:
        return Shader::OutputTopology::LineStrip;
    default:
        return Shader::OutputTopology::TriangleStrip;
    }
}

Shader::RuntimeInfo MakeRuntimeInfo(const GraphicsPipelineKey& key,
                                    const Shader::IR::Program& program,
                                    const Shader::IR::Program* previous_program,
                                    bool glasm_use_storage_buffers, bool use_assembly_shaders) {
    Shader::RuntimeInfo info;
    if (previous_program) {
        info.previous_stage_stores = previous_program->info.stores;
        info.previous_stage_legacy_stores_mapping = previous_program->info.legacy_stores_mapping;
    } else {
        // Mark all stores as available for vertex shaders
        info.previous_stage_stores.mask.set();
    }
    switch (program.stage) {
    case Shader::Stage::VertexB:
    case Shader::Stage::Geometry:
        if (!use_assembly_shaders && key.xfb_enabled != 0) {
            auto [varyings, count] = VideoCommon::MakeTransformFeedbackVaryings(key.xfb_state);
            info.xfb_varyings = varyings;
            info.xfb_count = count;
        }
        break;
    case Shader::Stage::TessellationEval:
        // Flip the face, as OpenGL's drawing is flipped.
        info.tess_clockwise = key.tessellation_clockwise == 0;
        info.tess_primitive = [&key] {
            switch (key.tessellation_primitive) {
            case Maxwell::Tessellation::DomainType::Isolines:
                return Shader::TessPrimitive::Isolines;
            case Maxwell::Tessellation::DomainType::Triangles:
                return Shader::TessPrimitive::Triangles;
            case Maxwell::Tessellation::DomainType::Quads:
                return Shader::TessPrimitive::Quads;
            }
            ASSERT(false);
            return Shader::TessPrimitive::Triangles;
        }();
        info.tess_spacing = [&] {
            switch (key.tessellation_spacing) {
            case Maxwell::Tessellation::Spacing::Integer:
                return Shader::TessSpacing::Equal;
            case Maxwell::Tessellation::Spacing::FractionalOdd:
                return Shader::TessSpacing::FractionalOdd;
            case Maxwell::Tessellation::Spacing::FractionalEven:
                return Shader::TessSpacing::FractionalEven;
            }
            ASSERT(false);
            return Shader::TessSpacing::Equal;
        }();
        break;
    case Shader::Stage::Fragment:
        info.force_early_z = key.early_z != 0;
        break;
    default:
        break;
    }
    switch (key.gs_input_topology) {
    case Maxwell::PrimitiveTopology::Points:
        info.input_topology = Shader::InputTopology::Points;
        break;
    case Maxwell::PrimitiveTopology::Lines:
    case Maxwell::PrimitiveTopology::LineLoop:
    case Maxwell::PrimitiveTopology::LineStrip:
        info.input_topology = Shader::InputTopology::Lines;
        break;
    case Maxwell::PrimitiveTopology::Triangles:
    case Maxwell::PrimitiveTopology::TriangleStrip:
    case Maxwell::PrimitiveTopology::TriangleFan:
    case Maxwell::PrimitiveTopology::Quads:
    case Maxwell::PrimitiveTopology::QuadStrip:
    case Maxwell::PrimitiveTopology::Polygon:
    case Maxwell::PrimitiveTopology::Patches:
        info.input_topology = Shader::InputTopology::Triangles;
        break;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        info.input_topology = Shader::InputTopology::LinesAdjacency;
        break;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        info.input_topology = Shader::InputTopology::TrianglesAdjacency;
        break;
    }
    info.glasm_use_storage_buffers = glasm_use_storage_buffers;
    return info;
}

void SetXfbState(VideoCommon::TransformFeedbackState& state, const Maxwell& regs) {
    std::ranges::transform(regs.transform_feedback.controls, state.layouts.begin(),
                           [](const auto& layout) {
                               return VideoCommon::TransformFeedbackState::Layout{
                                   .stream = layout.stream,
                                   .varying_count = layout.varying_count,
                                   .stride = layout.stride,
                               };
                           });
    state.varyings = regs.stream_out_layout;
}
} // Anonymous namespace

ShaderCache::ShaderCache(Tegra::MaxwellDeviceMemoryManager& device_memory_,
                         Core::Frontend::EmuWindow& emu_window_, const Device& device_,
                         TextureCache& texture_cache_, BufferCache& buffer_cache_,
                         ProgramManager& program_manager_, StateTracker& state_tracker_,
                         VideoCore::ShaderNotify& shader_notify_)
    : VideoCommon::ShaderCache{device_memory_}, emu_window{emu_window_}, device{device_},
      texture_cache{texture_cache_}, buffer_cache{buffer_cache_}, program_manager{program_manager_},
      state_tracker{state_tracker_}, shader_notify{shader_notify_},
      use_asynchronous_shaders{device.UseAsynchronousShaders()},
      strict_context_required{device.StrictContextRequired()},
      profile{
          .supported_spirv = 0x00010000,

          .unified_descriptor_binding = false,
          .support_descriptor_aliasing = false,
          .support_int8 = false,
          .support_int16 = false,
          .support_int64 = device.HasShaderInt64(),
          .support_vertex_instance_id = true,
          .support_float_controls = false,
          .support_separate_denorm_behavior = false,
          .support_separate_rounding_mode = false,
          .support_fp16_denorm_preserve = false,
          .support_fp32_denorm_preserve = false,
          .support_fp16_denorm_flush = false,
          .support_fp32_denorm_flush = false,
          .support_fp16_signed_zero_nan_preserve = false,
          .support_fp32_signed_zero_nan_preserve = false,
          .support_fp64_signed_zero_nan_preserve = false,
          .support_explicit_workgroup_layout = false,
          .support_vote = true,
          .support_viewport_index_layer_non_geometry =
              device.HasNvViewportArray2() || device.HasVertexViewportLayer(),
          .support_viewport_mask = device.HasNvViewportArray2(),
          .support_typeless_image_loads = device.HasImageLoadFormatted(),
          .support_demote_to_helper_invocation = false,
          .support_int64_atomics = false,
          .support_derivative_control = device.HasDerivativeControl(),
          .support_geometry_shader_passthrough = device.HasGeometryShaderPassthrough(),
          .support_native_ndc = true,
          .support_gl_nv_gpu_shader_5 = device.HasNvGpuShader5(),
          .support_gl_amd_gpu_shader_half_float = device.HasAmdShaderHalfFloat(),
          .support_gl_texture_shadow_lod = device.HasTextureShadowLod(),
          .support_gl_warp_intrinsics = false,
          .support_gl_variable_aoffi = device.HasVariableAoffi(),
          .support_gl_sparse_textures = device.HasSparseTexture2(),
          .support_gl_derivative_control = device.HasDerivativeControl(),
          .support_geometry_streams = true,

          .warp_size_potentially_larger_than_guest = device.IsWarpSizePotentiallyLargerThanGuest(),

          .lower_left_origin_mode = true,
          .need_declared_frag_colors = true,
          .need_fastmath_off = device.NeedsFastmathOff(),
          .need_gather_subpixel_offset = device.IsAmd() || device.IsIntel(),

          .has_broken_spirv_clamp = true,
          .has_broken_unsigned_image_offsets = true,
          .has_broken_signed_operations = true,
          .has_broken_fp16_float_controls = false,
          .has_gl_component_indexing_bug = device.HasComponentIndexingBug(),
          .has_gl_precise_bug = device.HasPreciseBug(),
          .has_gl_cbuf_ftou_bug = device.HasCbufFtouBug(),
          .has_gl_bool_ref_bug = device.HasBoolRefBug(),
          .ignore_nan_fp_comparisons = true,
          .gl_max_compute_smem_size = device.GetMaxComputeSharedMemorySize(),
          .min_ssbo_alignment = device.GetShaderStorageBufferAlignment(),
          .max_user_clip_distances = 8,
      },
      host_info{
          .support_float64 = true,
          .support_float16 = false,
          .support_int64 = device.HasShaderInt64(),
          .needs_demote_reorder = device.IsAmd(),
          .support_snorm_render_buffer = false,
          .support_viewport_index_layer = device.HasVertexViewportLayer(),
          .min_ssbo_alignment = static_cast<u32>(device.GetShaderStorageBufferAlignment()),
          .support_geometry_shader_passthrough = device.HasGeometryShaderPassthrough(),
          .support_conditional_barrier = device.SupportsConditionalBarriers(),
      } {
    if (use_asynchronous_shaders) {
        workers = CreateWorkers();
    }
}

ShaderCache::~ShaderCache() = default;

void ShaderCache::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                    const VideoCore::DiskResourceLoadCallback& callback) {
    if (title_id == 0) {
        return;
    }
    const auto shader_dir{Common::FS::GetYuzuPath(Common::FS::YuzuPath::ShaderDir)};
    const auto base_dir{shader_dir / fmt::format("{:016x}", title_id)};
    if (!Common::FS::CreateDir(shader_dir) || !Common::FS::CreateDir(base_dir)) {
        LOG_ERROR(Common_Filesystem, "Failed to create shader cache directories");
        return;
    }
    shader_cache_filename = base_dir / "opengl.bin";

    if (!workers && !strict_context_required) {
        workers = CreateWorkers();
    }
    std::optional<Context> strict_context;
    if (strict_context_required) {
        strict_context.emplace(emu_window);
    }

    struct {
        std::mutex mutex;
        size_t total{};
        size_t built{};
        bool has_loaded{};
    } state;

    const auto queue_work{[&](Common::UniqueFunction<void, Context*>&& work) {
        if (strict_context_required) {
            work(&strict_context.value());
        } else {
            workers->QueueWork(std::move(work));
        }
    }};
    const auto load_compute{[&](std::ifstream& file, FileEnvironment env) {
        ComputePipelineKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));
        queue_work([this, key, env_ = std::move(env), &state, &callback](Context* ctx) mutable {
            ctx->pools.ReleaseContents();
            auto pipeline{CreateComputePipeline(ctx->pools, key, env_, true)};
            std::scoped_lock lock{state.mutex};
            if (pipeline) {
                compute_cache.emplace(key, std::move(pipeline));
            }
            ++state.built;
            if (state.has_loaded) {
                callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
            }
        });
        ++state.total;
    }};
    const auto load_graphics{[&](std::ifstream& file, std::vector<FileEnvironment> envs) {
        GraphicsPipelineKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));
        queue_work([this, key, envs_ = std::move(envs), &state, &callback](Context* ctx) mutable {
            boost::container::static_vector<Shader::Environment*, 5> env_ptrs;
            for (auto& env : envs_) {
                env_ptrs.push_back(&env);
            }
            ctx->pools.ReleaseContents();
            auto pipeline{CreateGraphicsPipeline(ctx->pools, key, MakeSpan(env_ptrs), false, true)};
            std::scoped_lock lock{state.mutex};
            if (pipeline) {
                graphics_cache.emplace(key, std::move(pipeline));
            }
            ++state.built;
            if (state.has_loaded) {
                callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
            }
        });
        ++state.total;
    }};
    LoadPipelines(stop_loading, shader_cache_filename, CACHE_VERSION, load_compute, load_graphics);

    LOG_INFO(Render_OpenGL, "Total Pipeline Count: {}", state.total);

    std::unique_lock lock{state.mutex};
    callback(VideoCore::LoadCallbackStage::Build, 0, state.total);
    state.has_loaded = true;
    lock.unlock();

    if (strict_context_required) {
        return;
    }
    workers->WaitForRequests(stop_loading);
    if (!use_asynchronous_shaders) {
        workers.reset();
    }
}

GraphicsPipeline* ShaderCache::CurrentGraphicsPipeline() {
    if (!RefreshStages(graphics_key.unique_hashes)) {
        current_pipeline = nullptr;
        return nullptr;
    }
    const auto& regs{maxwell3d->regs};
    graphics_key.raw = 0;
    graphics_key.early_z.Assign(regs.mandated_early_z != 0 ? 1 : 0);
    graphics_key.gs_input_topology.Assign(maxwell3d->draw_manager->GetDrawState().topology);
    graphics_key.tessellation_primitive.Assign(regs.tessellation.params.domain_type.Value());
    graphics_key.tessellation_spacing.Assign(regs.tessellation.params.spacing.Value());
    graphics_key.tessellation_clockwise.Assign(
        regs.tessellation.params.output_primitives.Value() ==
        Maxwell::Tessellation::OutputPrimitives::Triangles_CW);
    graphics_key.xfb_enabled.Assign(regs.transform_feedback_enabled != 0 ? 1 : 0);
    graphics_key.app_stage.Assign(maxwell3d->engine_state);
    if (graphics_key.xfb_enabled) {
        SetXfbState(graphics_key.xfb_state, regs);
    }
    if (current_pipeline && graphics_key == current_pipeline->Key()) {
        return BuiltPipeline(current_pipeline);
    }
    return CurrentGraphicsPipelineSlowPath();
}

GraphicsPipeline* ShaderCache::CurrentGraphicsPipelineSlowPath() {
    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& pipeline{pair->second};
    if (is_new) {
        pipeline = CreateGraphicsPipeline();
    }
    if (!pipeline) {
        return nullptr;
    }
    current_pipeline = pipeline.get();
    return BuiltPipeline(current_pipeline);
}

GraphicsPipeline* ShaderCache::BuiltPipeline(GraphicsPipeline* pipeline) const noexcept {
    if (pipeline->IsBuilt()) {
        return pipeline;
    }
    if (!use_asynchronous_shaders) {
        return pipeline;
    }
    // If something is using depth, we can assume that games are not rendering anything which
    // will be used one time.
    if (maxwell3d->regs.zeta_enable) {
        return nullptr;
    }
    // If games are using a small index count, we can assume these are full screen quads.
    // Usually these shaders are only used once for building textures so we can assume they
    // can't be built async
    const auto& draw_state = maxwell3d->draw_manager->GetDrawState();
    if (draw_state.index_buffer.count <= 6 || draw_state.vertex_buffer.count <= 6) {
        return pipeline;
    }
    return nullptr;
}

ComputePipeline* ShaderCache::CurrentComputePipeline() {
    const VideoCommon::ShaderInfo* const shader{ComputeShader()};
    if (!shader) {
        return nullptr;
    }
    const auto& qmd{kepler_compute->launch_description};
    const ComputePipelineKey key{
        .unique_hash = shader->unique_hash,
        .shared_memory_size = qmd.shared_alloc,
        .workgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return pipeline.get();
    }
    pipeline = CreateComputePipeline(key, shader);
    return pipeline.get();
}

std::unique_ptr<GraphicsPipeline> ShaderCache::CreateGraphicsPipeline() {
    GraphicsEnvironments environments;
    GetGraphicsEnvironments(environments, graphics_key.unique_hashes);

    main_pools.ReleaseContents();
    auto pipeline{CreateGraphicsPipeline(main_pools, graphics_key, environments.Span(),
                                         use_asynchronous_shaders)};
    if (!pipeline || shader_cache_filename.empty()) {
        return pipeline;
    }
    boost::container::static_vector<const GenericEnvironment*, Maxwell::MaxShaderProgram> env_ptrs;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] != 0) {
            env_ptrs.push_back(&environments.envs[index]);
        }
    }
    SerializePipeline(graphics_key, env_ptrs, shader_cache_filename, CACHE_VERSION);
    return pipeline;
}

std::unique_ptr<GraphicsPipeline> ShaderCache::CreateGraphicsPipeline(
    ShaderContext::ShaderPools& pools, const GraphicsPipelineKey& key,
    std::span<Shader::Environment* const> envs, bool use_shader_workers,
    bool force_context_flush) try {
    auto hash = key.Hash();
    LOG_INFO(Render_OpenGL, "0x{:016x}", hash);
    size_t env_index{};
    u32 total_storage_buffers{};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;
    const bool uses_vertex_a{key.unique_hashes[0] != 0};
    const bool uses_vertex_b{key.unique_hashes[1] != 0};

    // Layer passthrough generation for devices without GL_ARB_shader_viewport_layer_array
    Shader::IR::Program* layer_source_program{};

    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        const bool is_emulated_stage = layer_source_program != nullptr &&
                                       index == static_cast<u32>(Maxwell::ShaderType::Geometry);
        if (key.unique_hashes[index] == 0 && is_emulated_stage) {
            auto topology = MaxwellToOutputTopology(key.gs_input_topology);
            programs[index] = GenerateGeometryPassthrough(pools.inst, pools.block, host_info,
                                                          *layer_source_program, topology);
            continue;
        }
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{static_cast<u32>(env.StartAddress() + sizeof(Shader::ProgramHeader))};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset, index == 0);

        if (Settings::values.dump_shaders) {
            env.Dump(hash, key.unique_hashes[index]);
        }

        if (!uses_vertex_a || index != 1) {
            // Normal path
            programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg, host_info);

            total_storage_buffers +=
                Shader::NumDescriptors(programs[index].info.storage_buffers_descriptors);
        } else {
            // VertexB path when VertexA is present.
            auto& program_va{programs[0]};
            auto program_vb{TranslateProgram(pools.inst, pools.block, env, cfg, host_info)};
            total_storage_buffers +=
                Shader::NumDescriptors(program_vb.info.storage_buffers_descriptors);
            programs[index] = MergeDualVertexPrograms(program_va, program_vb, env);
        }

        if (programs[index].info.requires_layer_emulation) {
            layer_source_program = &programs[index];
        }
    }
    const u32 glasm_storage_buffer_limit{device.GetMaxGLASMStorageBufferBlocks()};
    const bool glasm_use_storage_buffers{total_storage_buffers <= glasm_storage_buffer_limit};

    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};

    std::array<std::string, 5> sources;
    std::array<std::vector<u32>, 5> sources_spirv;
    Shader::Backend::Bindings binding;
    Shader::IR::Program* previous_program{};
    const bool use_glasm{device.UseAssemblyShaders()};
    const size_t first_index = uses_vertex_a && uses_vertex_b ? 1 : 0;
    for (size_t index = first_index; index < Maxwell::MaxShaderProgram; ++index) {
        const bool is_emulated_stage = layer_source_program != nullptr &&
                                       index == static_cast<u32>(Maxwell::ShaderType::Geometry);
        if (key.unique_hashes[index] == 0 && !is_emulated_stage) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        const auto runtime_info{
            MakeRuntimeInfo(key, program, previous_program, glasm_use_storage_buffers, use_glasm)};
        switch (device.GetShaderBackend()) {
        case Settings::ShaderBackend::Glsl:
            ConvertLegacyToGeneric(program, runtime_info);
            sources[stage_index] = EmitGLSL(profile, runtime_info, program, binding);
            break;
        case Settings::ShaderBackend::Glasm:
            sources[stage_index] = EmitGLASM(profile, runtime_info, program, binding);
            break;
        case Settings::ShaderBackend::SpirV:
            ConvertLegacyToGeneric(program, runtime_info);
            sources_spirv[stage_index] = EmitSPIRV(profile, runtime_info, program, binding);
            break;
        }
        previous_program = &program;
    }
    auto* const thread_worker{use_shader_workers ? workers.get() : nullptr};
    return std::make_unique<GraphicsPipeline>(device, texture_cache, buffer_cache, program_manager,
                                              state_tracker, thread_worker, &shader_notify, sources,
                                              sources_spirv, infos, key, force_context_flush);

} catch (Shader::Exception& exception) {
    LOG_ERROR(Render_OpenGL, "{}", exception.what());
    return nullptr;
}

std::unique_ptr<ComputePipeline> ShaderCache::CreateComputePipeline(
    const ComputePipelineKey& key, const VideoCommon::ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute->regs.code_loc.Address()};
    const auto& qmd{kepler_compute->launch_description};
    ComputeEnvironment env{*kepler_compute, *gpu_memory, program_base, qmd.program_start};
    env.SetCachedSize(shader->size_bytes);

    main_pools.ReleaseContents();
    auto pipeline{CreateComputePipeline(main_pools, key, env)};
    if (!pipeline || shader_cache_filename.empty()) {
        return pipeline;
    }
    SerializePipeline(key, std::array<const GenericEnvironment*, 1>{&env}, shader_cache_filename,
                      CACHE_VERSION);
    return pipeline;
}

std::unique_ptr<ComputePipeline> ShaderCache::CreateComputePipeline(
    ShaderContext::ShaderPools& pools, const ComputePipelineKey& key, Shader::Environment& env,
    bool force_context_flush) try {
    auto hash = key.Hash();
    LOG_INFO(Render_OpenGL, "0x{:016x}", hash);

    Shader::Maxwell::Flow::CFG cfg{env, pools.flow_block, env.StartAddress()};

    if (Settings::values.dump_shaders) {
        env.Dump(hash, key.unique_hash);
    }

    auto program{TranslateProgram(pools.inst, pools.block, env, cfg, host_info)};
    const u32 num_storage_buffers{Shader::NumDescriptors(program.info.storage_buffers_descriptors)};
    Shader::RuntimeInfo info;
    info.glasm_use_storage_buffers = num_storage_buffers <= device.GetMaxGLASMStorageBufferBlocks();

    std::string code{};
    std::vector<u32> code_spirv;
    switch (device.GetShaderBackend()) {
    case Settings::ShaderBackend::Glsl:
        code = EmitGLSL(profile, program);
        break;
    case Settings::ShaderBackend::Glasm:
        code = EmitGLASM(profile, info, program);
        break;
    case Settings::ShaderBackend::SpirV:
        code_spirv = EmitSPIRV(profile, program);
        break;
    }

    return std::make_unique<ComputePipeline>(device, texture_cache, buffer_cache, program_manager,
                                             program.info, code, code_spirv, force_context_flush);
} catch (Shader::Exception& exception) {
    LOG_ERROR(Render_OpenGL, "{}", exception.what());
    return nullptr;
}

std::unique_ptr<ShaderWorker> ShaderCache::CreateWorkers() const {
    return std::make_unique<ShaderWorker>(std::max(std::thread::hardware_concurrency(), 2U) - 1,
                                          "GlShaderBuilder",
                                          [this] { return Context{emu_window}; });
}

} // namespace OpenGL

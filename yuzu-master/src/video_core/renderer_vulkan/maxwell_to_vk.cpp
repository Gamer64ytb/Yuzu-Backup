// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iterator>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan::MaxwellToVK {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

namespace Sampler {

VkFilter Filter(Tegra::Texture::TextureFilter filter) {
    switch (filter) {
    case Tegra::Texture::TextureFilter::Nearest:
        return VK_FILTER_NEAREST;
    case Tegra::Texture::TextureFilter::Linear:
        return VK_FILTER_LINEAR;
    }
    ASSERT_MSG(false, "Invalid sampler filter={}", filter);
    return {};
}

VkSamplerMipmapMode MipmapMode(Tegra::Texture::TextureMipmapFilter mipmap_filter) {
    switch (mipmap_filter) {
    case Tegra::Texture::TextureMipmapFilter::None:
        // There are no Vulkan filter modes that directly correspond to OpenGL minification filters
        // of GL_LINEAR or GL_NEAREST, but they can be emulated using
        // VK_SAMPLER_MIPMAP_MODE_NEAREST, minLod = 0, and maxLod = 0.25, and using minFilter =
        // VK_FILTER_LINEAR or minFilter = VK_FILTER_NEAREST, respectively.
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case Tegra::Texture::TextureMipmapFilter::Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case Tegra::Texture::TextureMipmapFilter::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
    ASSERT_MSG(false, "Invalid sampler mipmap mode={}", mipmap_filter);
    return {};
}

VkSamplerAddressMode WrapMode(const Device& device, Tegra::Texture::WrapMode wrap_mode,
                              Tegra::Texture::TextureFilter filter) {
    switch (wrap_mode) {
    case Tegra::Texture::WrapMode::Wrap:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case Tegra::Texture::WrapMode::Mirror:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case Tegra::Texture::WrapMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case Tegra::Texture::WrapMode::Border:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case Tegra::Texture::WrapMode::Clamp:
        if (device.GetDriverID() == VK_DRIVER_ID_NVIDIA_PROPRIETARY) {
            // Nvidia's Vulkan driver defaults to GL_CLAMP on invalid enumerations, we can hack this
            // by sending an invalid enumeration.
            return static_cast<VkSamplerAddressMode>(0xcafe);
        }
        // TODO(Rodrigo): Emulate GL_CLAMP properly on other vendors
        switch (filter) {
        case Tegra::Texture::TextureFilter::Nearest:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case Tegra::Texture::TextureFilter::Linear:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        }
        ASSERT(false);
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case Tegra::Texture::WrapMode::MirrorOnceClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    case Tegra::Texture::WrapMode::MirrorOnceBorder:
        UNIMPLEMENTED();
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        UNIMPLEMENTED_MSG("Unimplemented wrap mode={}", wrap_mode);
        return {};
    }
}

VkCompareOp DepthCompareFunction(Tegra::Texture::DepthCompareFunc depth_compare_func) {
    switch (depth_compare_func) {
    case Tegra::Texture::DepthCompareFunc::Never:
        return VK_COMPARE_OP_NEVER;
    case Tegra::Texture::DepthCompareFunc::Less:
        return VK_COMPARE_OP_LESS;
    case Tegra::Texture::DepthCompareFunc::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case Tegra::Texture::DepthCompareFunc::Equal:
        return VK_COMPARE_OP_EQUAL;
    case Tegra::Texture::DepthCompareFunc::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case Tegra::Texture::DepthCompareFunc::Greater:
        return VK_COMPARE_OP_GREATER;
    case Tegra::Texture::DepthCompareFunc::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case Tegra::Texture::DepthCompareFunc::Always:
        return VK_COMPARE_OP_ALWAYS;
    }
    UNIMPLEMENTED_MSG("Unimplemented sampler depth compare function={}", depth_compare_func);
    return {};
}

} // namespace Sampler

namespace {
constexpr u32 Attachable = 1 << 0;
constexpr u32 Storage = 1 << 1;

struct FormatTuple {
    VkFormat format; ///< Vulkan format
    int usage = 0;   ///< Describes image format usage
} constexpr tex_format_tuples[] = {
    {VK_FORMAT_A8B8G8R8_UNORM_PACK32, Attachable | Storage},    // A8B8G8R8_UNORM
    {VK_FORMAT_A8B8G8R8_SNORM_PACK32, Attachable | Storage},    // A8B8G8R8_SNORM
    {VK_FORMAT_A8B8G8R8_SINT_PACK32, Attachable | Storage},     // A8B8G8R8_SINT
    {VK_FORMAT_A8B8G8R8_UINT_PACK32, Attachable | Storage},     // A8B8G8R8_UINT
    {VK_FORMAT_R5G6B5_UNORM_PACK16, Attachable},                // R5G6B5_UNORM
    {VK_FORMAT_B5G6R5_UNORM_PACK16},                            // B5G6R5_UNORM
    {VK_FORMAT_A1R5G5B5_UNORM_PACK16, Attachable},              // A1R5G5B5_UNORM
    {VK_FORMAT_A2B10G10R10_UNORM_PACK32, Attachable | Storage}, // A2B10G10R10_UNORM
    {VK_FORMAT_A2B10G10R10_UINT_PACK32, Attachable | Storage},  // A2B10G10R10_UINT
    {VK_FORMAT_A2R10G10B10_UNORM_PACK32, Attachable},           // A2R10G10B10_UNORM
    {VK_FORMAT_A1R5G5B5_UNORM_PACK16, Attachable},         // A1B5G5R5_UNORM (flipped with swizzle)
    {VK_FORMAT_R5G5B5A1_UNORM_PACK16},                     // A5B5G5R1_UNORM (specially swizzled)
    {VK_FORMAT_R8_UNORM, Attachable | Storage},            // R8_UNORM
    {VK_FORMAT_R8_SNORM, Attachable | Storage},            // R8_SNORM
    {VK_FORMAT_R8_SINT, Attachable | Storage},             // R8_SINT
    {VK_FORMAT_R8_UINT, Attachable | Storage},             // R8_UINT
    {VK_FORMAT_R16G16B16A16_SFLOAT, Attachable | Storage}, // R16G16B16A16_FLOAT
    {VK_FORMAT_R16G16B16A16_UNORM, Attachable | Storage},  // R16G16B16A16_UNORM
    {VK_FORMAT_R16G16B16A16_SNORM, Attachable | Storage},  // R16G16B16A16_SNORM
    {VK_FORMAT_R16G16B16A16_SINT, Attachable | Storage},   // R16G16B16A16_SINT
    {VK_FORMAT_R16G16B16A16_UINT, Attachable | Storage},   // R16G16B16A16_UINT
    {VK_FORMAT_B10G11R11_UFLOAT_PACK32, Attachable | Storage}, // B10G11R11_FLOAT
    {VK_FORMAT_R32G32B32A32_UINT, Attachable | Storage},       // R32G32B32A32_UINT
    {VK_FORMAT_BC1_RGBA_UNORM_BLOCK},                          // BC1_RGBA_UNORM
    {VK_FORMAT_BC2_UNORM_BLOCK},                               // BC2_UNORM
    {VK_FORMAT_BC3_UNORM_BLOCK},                               // BC3_UNORM
    {VK_FORMAT_BC4_UNORM_BLOCK},                               // BC4_UNORM
    {VK_FORMAT_BC4_SNORM_BLOCK},                               // BC4_SNORM
    {VK_FORMAT_BC5_UNORM_BLOCK},                               // BC5_UNORM
    {VK_FORMAT_BC5_SNORM_BLOCK},                               // BC5_SNORM
    {VK_FORMAT_BC7_UNORM_BLOCK},                               // BC7_UNORM
    {VK_FORMAT_BC6H_UFLOAT_BLOCK},                             // BC6H_UFLOAT
    {VK_FORMAT_BC6H_SFLOAT_BLOCK},                             // BC6H_SFLOAT
    {VK_FORMAT_ASTC_4x4_UNORM_BLOCK},                          // ASTC_2D_4X4_UNORM
    {VK_FORMAT_B8G8R8A8_UNORM, Attachable | Storage},          // B8G8R8A8_UNORM
    {VK_FORMAT_R32G32B32A32_SFLOAT, Attachable | Storage},     // R32G32B32A32_FLOAT
    {VK_FORMAT_R32G32B32A32_SINT, Attachable | Storage},       // R32G32B32A32_SINT
    {VK_FORMAT_R32G32_SFLOAT, Attachable | Storage},           // R32G32_FLOAT
    {VK_FORMAT_R32G32_SINT, Attachable | Storage},             // R32G32_SINT
    {VK_FORMAT_R32_SFLOAT, Attachable | Storage},              // R32_FLOAT
    {VK_FORMAT_R16_SFLOAT, Attachable | Storage},              // R16_FLOAT
    {VK_FORMAT_R16_UNORM, Attachable | Storage},               // R16_UNORM
    {VK_FORMAT_R16_SNORM, Attachable | Storage},               // R16_SNORM
    {VK_FORMAT_R16_UINT, Attachable | Storage},                // R16_UINT
    {VK_FORMAT_R16_SINT, Attachable | Storage},                // R16_SINT
    {VK_FORMAT_R16G16_UNORM, Attachable | Storage},            // R16G16_UNORM
    {VK_FORMAT_R16G16_SFLOAT, Attachable | Storage},           // R16G16_FLOAT
    {VK_FORMAT_R16G16_UINT, Attachable | Storage},             // R16G16_UINT
    {VK_FORMAT_R16G16_SINT, Attachable | Storage},             // R16G16_SINT
    {VK_FORMAT_R16G16_SNORM, Attachable | Storage},            // R16G16_SNORM
    {VK_FORMAT_R32G32B32_SFLOAT},                              // R32G32B32_FLOAT
    {VK_FORMAT_A8B8G8R8_SRGB_PACK32, Attachable},              // A8B8G8R8_SRGB
    {VK_FORMAT_R8G8_UNORM, Attachable | Storage},              // R8G8_UNORM
    {VK_FORMAT_R8G8_SNORM, Attachable | Storage},              // R8G8_SNORM
    {VK_FORMAT_R8G8_SINT, Attachable | Storage},               // R8G8_SINT
    {VK_FORMAT_R8G8_UINT, Attachable | Storage},               // R8G8_UINT
    {VK_FORMAT_R32G32_UINT, Attachable | Storage},             // R32G32_UINT
    {VK_FORMAT_R16G16B16A16_SFLOAT, Attachable | Storage},     // R16G16B16X16_FLOAT
    {VK_FORMAT_R32_UINT, Attachable | Storage},                // R32_UINT
    {VK_FORMAT_R32_SINT, Attachable | Storage},                // R32_SINT
    {VK_FORMAT_ASTC_8x8_UNORM_BLOCK},                          // ASTC_2D_8X8_UNORM
    {VK_FORMAT_ASTC_8x5_UNORM_BLOCK},                          // ASTC_2D_8X5_UNORM
    {VK_FORMAT_ASTC_5x4_UNORM_BLOCK},                          // ASTC_2D_5X4_UNORM
    {VK_FORMAT_B8G8R8A8_SRGB, Attachable},                     // B8G8R8A8_SRGB
    {VK_FORMAT_BC1_RGBA_SRGB_BLOCK},                           // BC1_RGBA_SRGB
    {VK_FORMAT_BC2_SRGB_BLOCK},                                // BC2_SRGB
    {VK_FORMAT_BC3_SRGB_BLOCK},                                // BC3_SRGB
    {VK_FORMAT_BC7_SRGB_BLOCK},                                // BC7_SRGB
    {VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT},                     // A4B4G4R4_UNORM
    {VK_FORMAT_R4G4_UNORM_PACK8},                              // G4R4_UNORM
    {VK_FORMAT_ASTC_4x4_SRGB_BLOCK},                           // ASTC_2D_4X4_SRGB
    {VK_FORMAT_ASTC_8x8_SRGB_BLOCK},                           // ASTC_2D_8X8_SRGB
    {VK_FORMAT_ASTC_8x5_SRGB_BLOCK},                           // ASTC_2D_8X5_SRGB
    {VK_FORMAT_ASTC_5x4_SRGB_BLOCK},                           // ASTC_2D_5X4_SRGB
    {VK_FORMAT_ASTC_5x5_UNORM_BLOCK},                          // ASTC_2D_5X5_UNORM
    {VK_FORMAT_ASTC_5x5_SRGB_BLOCK},                           // ASTC_2D_5X5_SRGB
    {VK_FORMAT_ASTC_10x8_UNORM_BLOCK},                         // ASTC_2D_10X8_UNORM
    {VK_FORMAT_ASTC_10x8_SRGB_BLOCK},                          // ASTC_2D_10X8_SRGB
    {VK_FORMAT_ASTC_6x6_UNORM_BLOCK},                          // ASTC_2D_6X6_UNORM
    {VK_FORMAT_ASTC_6x6_SRGB_BLOCK},                           // ASTC_2D_6X6_SRGB
    {VK_FORMAT_ASTC_10x6_UNORM_BLOCK},                         // ASTC_2D_10X6_UNORM
    {VK_FORMAT_ASTC_10x6_SRGB_BLOCK},                          // ASTC_2D_10X6_SRGB
    {VK_FORMAT_ASTC_10x5_UNORM_BLOCK},                         // ASTC_2D_10X5_UNORM
    {VK_FORMAT_ASTC_10x5_SRGB_BLOCK},                          // ASTC_2D_10X5_SRGB
    {VK_FORMAT_ASTC_10x10_UNORM_BLOCK},                        // ASTC_2D_10X10_UNORM
    {VK_FORMAT_ASTC_10x10_SRGB_BLOCK},                         // ASTC_2D_10X10_SRGB
    {VK_FORMAT_ASTC_12x10_UNORM_BLOCK},                        // ASTC_2D_12X10_UNORM
    {VK_FORMAT_ASTC_12x10_SRGB_BLOCK},                         // ASTC_2D_12X10_SRGB
    {VK_FORMAT_ASTC_12x12_UNORM_BLOCK},                        // ASTC_2D_12X12_UNORM
    {VK_FORMAT_ASTC_12x12_SRGB_BLOCK},                         // ASTC_2D_12X12_SRGB
    {VK_FORMAT_ASTC_8x6_UNORM_BLOCK},                          // ASTC_2D_8X6_UNORM
    {VK_FORMAT_ASTC_8x6_SRGB_BLOCK},                           // ASTC_2D_8X6_SRGB
    {VK_FORMAT_ASTC_6x5_UNORM_BLOCK},                          // ASTC_2D_6X5_UNORM
    {VK_FORMAT_ASTC_6x5_SRGB_BLOCK},                           // ASTC_2D_6X5_SRGB
    {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32},                        // E5B9G9R9_FLOAT

    // Depth formats
    {VK_FORMAT_D32_SFLOAT, Attachable},          // D32_FLOAT
    {VK_FORMAT_D16_UNORM, Attachable},           // D16_UNORM
    {VK_FORMAT_X8_D24_UNORM_PACK32, Attachable}, // X8_D24_UNORM

    // Stencil formats
    {VK_FORMAT_S8_UINT, Attachable}, // S8_UINT

    // DepthStencil formats
    {VK_FORMAT_D24_UNORM_S8_UINT, Attachable},  // D24_UNORM_S8_UINT
    {VK_FORMAT_D24_UNORM_S8_UINT, Attachable},  // S8_UINT_D24_UNORM (emulated)
    {VK_FORMAT_D32_SFLOAT_S8_UINT, Attachable}, // D32_FLOAT_S8_UINT
};
static_assert(std::size(tex_format_tuples) == VideoCore::Surface::MaxPixelFormat);

constexpr bool IsZetaFormat(PixelFormat pixel_format) {
    return pixel_format >= PixelFormat::MaxColorFormat &&
           pixel_format < PixelFormat::MaxDepthStencilFormat;
}

} // Anonymous namespace

FormatInfo SurfaceFormat(const Device& device, FormatType format_type, bool with_srgb,
                         PixelFormat pixel_format) {
    ASSERT(static_cast<size_t>(pixel_format) < std::size(tex_format_tuples));
    FormatTuple tuple = tex_format_tuples[static_cast<size_t>(pixel_format)];
    // Transcode on hardware that doesn't support ASTC natively
    if (!device.IsOptimalAstcSupported() && VideoCore::Surface::IsPixelFormatASTC(pixel_format)) {
        const bool is_srgb = with_srgb && VideoCore::Surface::IsPixelFormatSRGB(pixel_format);

        switch (Settings::values.astc_recompression.GetValue()) {
        case Settings::AstcRecompression::Uncompressed:
            if (is_srgb) {
                tuple.format = VK_FORMAT_A8B8G8R8_SRGB_PACK32;
            } else {
                tuple.format = VK_FORMAT_A8B8G8R8_UNORM_PACK32;
                tuple.usage |= Storage;
            }
            break;
        case Settings::AstcRecompression::Bc1:
            tuple.format = is_srgb ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            break;
        case Settings::AstcRecompression::Bc3:
            tuple.format = is_srgb ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
            break;
        }
    }
    // Transcode on hardware that doesn't support BCn natively
    if (!device.IsOptimalBcnSupported() && VideoCore::Surface::IsPixelFormatBCn(pixel_format)) {
        const bool is_srgb = with_srgb && VideoCore::Surface::IsPixelFormatSRGB(pixel_format);
        if (pixel_format == PixelFormat::BC4_SNORM) {
            tuple.format = VK_FORMAT_R8_SNORM;
        } else if (pixel_format == PixelFormat::BC4_UNORM) {
            tuple.format = VK_FORMAT_R8_UNORM;
        } else if (pixel_format == PixelFormat::BC5_SNORM) {
            tuple.format = VK_FORMAT_R8G8_SNORM;
        } else if (pixel_format == PixelFormat::BC5_UNORM) {
            tuple.format = VK_FORMAT_R8G8_UNORM;
        } else if (pixel_format == PixelFormat::BC6H_SFLOAT ||
                   pixel_format == PixelFormat::BC6H_UFLOAT) {
            tuple.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        } else if (is_srgb) {
            tuple.format = VK_FORMAT_A8B8G8R8_SRGB_PACK32;
        } else {
            tuple.format = VK_FORMAT_A8B8G8R8_UNORM_PACK32;
        }
    }
    const bool attachable = (tuple.usage & Attachable) != 0;
    const bool storage = (tuple.usage & Storage) != 0;

    VkFormatFeatureFlags usage{};
    switch (format_type) {
    case FormatType::Buffer:
        usage =
            VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT | VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;
        break;
    case FormatType::Linear:
    case FormatType::Optimal:
        usage = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
        if (attachable) {
            usage |= IsZetaFormat(pixel_format) ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        }
        if (storage) {
            usage |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        }
        break;
    }
    return {device.GetSupportedFormat(tuple.format, usage, format_type), attachable, storage};
}

VkShaderStageFlagBits ShaderStage(Shader::Stage stage) {
    switch (stage) {
    case Shader::Stage::VertexA:
    case Shader::Stage::VertexB:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case Shader::Stage::TessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case Shader::Stage::TessellationEval:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case Shader::Stage::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case Shader::Stage::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case Shader::Stage::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    }
    UNIMPLEMENTED_MSG("Unimplemented shader stage={}", stage);
    return {};
}

VkPrimitiveTopology PrimitiveTopology([[maybe_unused]] const Device& device,
                                      Maxwell::PrimitiveTopology topology) {
    switch (topology) {
    case Maxwell::PrimitiveTopology::Points:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case Maxwell::PrimitiveTopology::Lines:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Maxwell::PrimitiveTopology::LineLoop:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Maxwell::PrimitiveTopology::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case Maxwell::PrimitiveTopology::Triangles:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Maxwell::PrimitiveTopology::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Maxwell::PrimitiveTopology::TriangleFan:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
    case Maxwell::PrimitiveTopology::Quads:
    case Maxwell::PrimitiveTopology::QuadStrip:
        // TODO: Use VK_PRIMITIVE_TOPOLOGY_QUAD_LIST_EXT/VK_PRIMITIVE_TOPOLOGY_QUAD_STRIP_EXT
        // whenever it releases
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Maxwell::PrimitiveTopology::Patches:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    case Maxwell::PrimitiveTopology::Polygon:
        LOG_WARNING(Render_Vulkan, "Draw mode is Polygon with a polygon mode of lines should be a "
                                   "single body and not a bunch of triangles.");
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    }
    UNIMPLEMENTED_MSG("Unimplemented topology={}", topology);
    return {};
}

VkFormat VertexFormat(const Device& device, Maxwell::VertexAttribute::Type type,
                      Maxwell::VertexAttribute::Size size) {
    if (device.MustEmulateScaledFormats()) {
        if (type == Maxwell::VertexAttribute::Type::SScaled) {
            type = Maxwell::VertexAttribute::Type::SInt;
        } else if (type == Maxwell::VertexAttribute::Type::UScaled) {
            type = Maxwell::VertexAttribute::Type::UInt;
        }
    }

    const VkFormat format{([&]() {
        switch (type) {
        case Maxwell::VertexAttribute::Type::UnusedEnumDoNotUseBecauseItWillGoAway:
            ASSERT_MSG(false, "Invalid vertex attribute type!");
            break;
        case Maxwell::VertexAttribute::Type::UNorm:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R8:
            case Maxwell::VertexAttribute::Size::Size_A8:
                return VK_FORMAT_R8_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R8_G8:
            case Maxwell::VertexAttribute::Size::Size_G8_R8:
                return VK_FORMAT_R8G8_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
                return VK_FORMAT_R8G8B8_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
            case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_UNORM;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
                return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:
                break;
            }
            break;
        case Maxwell::VertexAttribute::Type::SNorm:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R8:
            case Maxwell::VertexAttribute::Size::Size_A8:
                return VK_FORMAT_R8_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R8_G8:
            case Maxwell::VertexAttribute::Size::Size_G8_R8:
                return VK_FORMAT_R8G8_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
                return VK_FORMAT_R8G8B8_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
            case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_SNORM;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_SNORM;
            case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
                return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
            default:
                break;
            }
            break;
        case Maxwell::VertexAttribute::Type::UScaled:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R8:
            case Maxwell::VertexAttribute::Size::Size_A8:
                return VK_FORMAT_R8_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R8_G8:
            case Maxwell::VertexAttribute::Size::Size_G8_R8:
                return VK_FORMAT_R8G8_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
                return VK_FORMAT_R8G8B8_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
            case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
                return VK_FORMAT_R8G8B8A8_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_USCALED;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_USCALED;
            case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
                return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
            default:
                break;
            }
            break;
        case Maxwell::VertexAttribute::Type::SScaled:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R8:
            case Maxwell::VertexAttribute::Size::Size_A8:
                return VK_FORMAT_R8_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R8_G8:
            case Maxwell::VertexAttribute::Size::Size_G8_R8:
                return VK_FORMAT_R8G8_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
                return VK_FORMAT_R8G8B8_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
            case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
                return VK_FORMAT_R8G8B8A8_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_SSCALED;
            case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
                return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
            default:
                break;
            }
            break;
        case Maxwell::VertexAttribute::Type::UInt:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R8:
            case Maxwell::VertexAttribute::Size::Size_A8:
                return VK_FORMAT_R8_UINT;
            case Maxwell::VertexAttribute::Size::Size_R8_G8:
            case Maxwell::VertexAttribute::Size::Size_G8_R8:
                return VK_FORMAT_R8G8_UINT;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
                return VK_FORMAT_R8G8B8_UINT;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
            case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
                return VK_FORMAT_R8G8B8A8_UINT;
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_UINT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_UINT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_UINT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_UINT;
            case Maxwell::VertexAttribute::Size::Size_R32:
                return VK_FORMAT_R32_UINT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32:
                return VK_FORMAT_R32G32_UINT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32_B32:
                return VK_FORMAT_R32G32B32_UINT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32_B32_A32:
                return VK_FORMAT_R32G32B32A32_UINT;
            case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
                return VK_FORMAT_A2B10G10R10_UINT_PACK32;
            default:
                break;
            }
            break;
        case Maxwell::VertexAttribute::Type::SInt:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R8:
            case Maxwell::VertexAttribute::Size::Size_A8:
                return VK_FORMAT_R8_SINT;
            case Maxwell::VertexAttribute::Size::Size_R8_G8:
            case Maxwell::VertexAttribute::Size::Size_G8_R8:
                return VK_FORMAT_R8G8_SINT;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
                return VK_FORMAT_R8G8B8_SINT;
            case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
            case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
                return VK_FORMAT_R8G8B8A8_SINT;
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_SINT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_SINT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_SINT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_SINT;
            case Maxwell::VertexAttribute::Size::Size_R32:
                return VK_FORMAT_R32_SINT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32:
                return VK_FORMAT_R32G32_SINT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32_B32:
                return VK_FORMAT_R32G32B32_SINT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32_B32_A32:
                return VK_FORMAT_R32G32B32A32_SINT;
            case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
                return VK_FORMAT_A2B10G10R10_SINT_PACK32;
            default:
                break;
            }
            break;
        case Maxwell::VertexAttribute::Type::Float:
            switch (size) {
            case Maxwell::VertexAttribute::Size::Size_R16:
                return VK_FORMAT_R16_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16:
                return VK_FORMAT_R16G16_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
                return VK_FORMAT_R16G16B16_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R32:
                return VK_FORMAT_R32_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32:
                return VK_FORMAT_R32G32_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32_B32:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_R32_G32_B32_A32:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case Maxwell::VertexAttribute::Size::Size_B10_G11_R11:
                return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            default:
                break;
            }
            break;
        }
        return VK_FORMAT_UNDEFINED;
    })()};

    if (format == VK_FORMAT_UNDEFINED) {
        UNIMPLEMENTED_MSG("Unimplemented vertex format of type={} and size={}", type, size);
    }

    return device.GetSupportedFormat(format, VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT,
                                     FormatType::Buffer);
}

VkCompareOp ComparisonOp(Maxwell::ComparisonOp comparison) {
    switch (comparison) {
    case Maxwell::ComparisonOp::Never_D3D:
    case Maxwell::ComparisonOp::Never_GL:
        return VK_COMPARE_OP_NEVER;
    case Maxwell::ComparisonOp::Less_D3D:
    case Maxwell::ComparisonOp::Less_GL:
        return VK_COMPARE_OP_LESS;
    case Maxwell::ComparisonOp::Equal_D3D:
    case Maxwell::ComparisonOp::Equal_GL:
        return VK_COMPARE_OP_EQUAL;
    case Maxwell::ComparisonOp::LessEqual_D3D:
    case Maxwell::ComparisonOp::LessEqual_GL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case Maxwell::ComparisonOp::Greater_D3D:
    case Maxwell::ComparisonOp::Greater_GL:
        return VK_COMPARE_OP_GREATER;
    case Maxwell::ComparisonOp::NotEqual_D3D:
    case Maxwell::ComparisonOp::NotEqual_GL:
        return VK_COMPARE_OP_NOT_EQUAL;
    case Maxwell::ComparisonOp::GreaterEqual_D3D:
    case Maxwell::ComparisonOp::GreaterEqual_GL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case Maxwell::ComparisonOp::Always_D3D:
    case Maxwell::ComparisonOp::Always_GL:
        return VK_COMPARE_OP_ALWAYS;
    }
    UNIMPLEMENTED_MSG("Unimplemented comparison op={}", comparison);
    return {};
}

VkIndexType IndexFormat(Maxwell::IndexFormat index_format) {
    switch (index_format) {
    case Maxwell::IndexFormat::UnsignedByte:
        return VK_INDEX_TYPE_UINT8_EXT;
    case Maxwell::IndexFormat::UnsignedShort:
        return VK_INDEX_TYPE_UINT16;
    case Maxwell::IndexFormat::UnsignedInt:
        return VK_INDEX_TYPE_UINT32;
    }
    UNIMPLEMENTED_MSG("Unimplemented index_format={}", index_format);
    return {};
}

VkStencilOp StencilOp(Maxwell::StencilOp::Op stencil_op) {
    switch (stencil_op) {
    case Maxwell::StencilOp::Op::Keep_D3D:
    case Maxwell::StencilOp::Op::Keep_GL:
        return VK_STENCIL_OP_KEEP;
    case Maxwell::StencilOp::Op::Zero_D3D:
    case Maxwell::StencilOp::Op::Zero_GL:
        return VK_STENCIL_OP_ZERO;
    case Maxwell::StencilOp::Op::Replace_D3D:
    case Maxwell::StencilOp::Op::Replace_GL:
        return VK_STENCIL_OP_REPLACE;
    case Maxwell::StencilOp::Op::IncrSaturate_D3D:
    case Maxwell::StencilOp::Op::IncrSaturate_GL:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case Maxwell::StencilOp::Op::DecrSaturate_D3D:
    case Maxwell::StencilOp::Op::DecrSaturate_GL:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case Maxwell::StencilOp::Op::Invert_D3D:
    case Maxwell::StencilOp::Op::Invert_GL:
        return VK_STENCIL_OP_INVERT;
    case Maxwell::StencilOp::Op::Incr_D3D:
    case Maxwell::StencilOp::Op::Incr_GL:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case Maxwell::StencilOp::Op::Decr_D3D:
    case Maxwell::StencilOp::Op::Decr_GL:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    UNIMPLEMENTED_MSG("Unimplemented stencil op={}", stencil_op);
    return {};
}

VkBlendOp BlendEquation(Maxwell::Blend::Equation equation) {
    switch (equation) {
    case Maxwell::Blend::Equation::Add_D3D:
    case Maxwell::Blend::Equation::Add_GL:
        return VK_BLEND_OP_ADD;
    case Maxwell::Blend::Equation::Subtract_D3D:
    case Maxwell::Blend::Equation::Subtract_GL:
        return VK_BLEND_OP_SUBTRACT;
    case Maxwell::Blend::Equation::ReverseSubtract_D3D:
    case Maxwell::Blend::Equation::ReverseSubtract_GL:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case Maxwell::Blend::Equation::Min_D3D:
    case Maxwell::Blend::Equation::Min_GL:
        return VK_BLEND_OP_MIN;
    case Maxwell::Blend::Equation::Max_D3D:
    case Maxwell::Blend::Equation::Max_GL:
        return VK_BLEND_OP_MAX;
    }
    UNIMPLEMENTED_MSG("Unimplemented blend equation={}", equation);
    return {};
}

VkBlendFactor BlendFactor(Maxwell::Blend::Factor factor) {
    switch (factor) {
    case Maxwell::Blend::Factor::Zero_D3D:
    case Maxwell::Blend::Factor::Zero_GL:
        return VK_BLEND_FACTOR_ZERO;
    case Maxwell::Blend::Factor::One_D3D:
    case Maxwell::Blend::Factor::One_GL:
        return VK_BLEND_FACTOR_ONE;
    case Maxwell::Blend::Factor::SourceColor_D3D:
    case Maxwell::Blend::Factor::SourceColor_GL:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case Maxwell::Blend::Factor::OneMinusSourceColor_D3D:
    case Maxwell::Blend::Factor::OneMinusSourceColor_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case Maxwell::Blend::Factor::SourceAlpha_D3D:
    case Maxwell::Blend::Factor::SourceAlpha_GL:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case Maxwell::Blend::Factor::OneMinusSourceAlpha_D3D:
    case Maxwell::Blend::Factor::OneMinusSourceAlpha_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case Maxwell::Blend::Factor::DestAlpha_D3D:
    case Maxwell::Blend::Factor::DestAlpha_GL:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case Maxwell::Blend::Factor::OneMinusDestAlpha_D3D:
    case Maxwell::Blend::Factor::OneMinusDestAlpha_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case Maxwell::Blend::Factor::DestColor_D3D:
    case Maxwell::Blend::Factor::DestColor_GL:
        return VK_BLEND_FACTOR_DST_COLOR;
    case Maxwell::Blend::Factor::OneMinusDestColor_D3D:
    case Maxwell::Blend::Factor::OneMinusDestColor_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case Maxwell::Blend::Factor::SourceAlphaSaturate_D3D:
    case Maxwell::Blend::Factor::SourceAlphaSaturate_GL:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case Maxwell::Blend::Factor::Source1Color_D3D:
    case Maxwell::Blend::Factor::Source1Color_GL:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case Maxwell::Blend::Factor::OneMinusSource1Color_D3D:
    case Maxwell::Blend::Factor::OneMinusSource1Color_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case Maxwell::Blend::Factor::Source1Alpha_D3D:
    case Maxwell::Blend::Factor::Source1Alpha_GL:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case Maxwell::Blend::Factor::OneMinusSource1Alpha_D3D:
    case Maxwell::Blend::Factor::OneMinusSource1Alpha_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    case Maxwell::Blend::Factor::BlendFactor_D3D:
    case Maxwell::Blend::Factor::ConstantColor_GL:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case Maxwell::Blend::Factor::OneMinusBlendFactor_D3D:
    case Maxwell::Blend::Factor::OneMinusConstantColor_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case Maxwell::Blend::Factor::BothSourceAlpha_D3D:
    case Maxwell::Blend::Factor::ConstantAlpha_GL:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case Maxwell::Blend::Factor::OneMinusBothSourceAlpha_D3D:
    case Maxwell::Blend::Factor::OneMinusConstantAlpha_GL:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    }
    UNIMPLEMENTED_MSG("Unimplemented blend factor={}", factor);
    return {};
}

VkFrontFace FrontFace(Maxwell::FrontFace front_face) {
    switch (front_face) {
    case Maxwell::FrontFace::ClockWise:
        return VK_FRONT_FACE_CLOCKWISE;
    case Maxwell::FrontFace::CounterClockWise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    UNIMPLEMENTED_MSG("Unimplemented front face={}", front_face);
    return {};
}

VkCullModeFlagBits CullFace(Maxwell::CullFace cull_face) {
    switch (cull_face) {
    case Maxwell::CullFace::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case Maxwell::CullFace::Back:
        return VK_CULL_MODE_BACK_BIT;
    case Maxwell::CullFace::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    }
    UNIMPLEMENTED_MSG("Unimplemented cull face={}", cull_face);
    return {};
}

VkPolygonMode PolygonMode(Maxwell::PolygonMode polygon_mode) {
    switch (polygon_mode) {
    case Maxwell::PolygonMode::Point:
        return VK_POLYGON_MODE_POINT;
    case Maxwell::PolygonMode::Line:
        return VK_POLYGON_MODE_LINE;
    case Maxwell::PolygonMode::Fill:
        return VK_POLYGON_MODE_FILL;
    }
    UNIMPLEMENTED_MSG("Unimplemented polygon mode={}", polygon_mode);
    return {};
}

VkComponentSwizzle SwizzleSource(Tegra::Texture::SwizzleSource swizzle) {
    switch (swizzle) {
    case Tegra::Texture::SwizzleSource::Zero:
        return VK_COMPONENT_SWIZZLE_ZERO;
    case Tegra::Texture::SwizzleSource::R:
        return VK_COMPONENT_SWIZZLE_R;
    case Tegra::Texture::SwizzleSource::G:
        return VK_COMPONENT_SWIZZLE_G;
    case Tegra::Texture::SwizzleSource::B:
        return VK_COMPONENT_SWIZZLE_B;
    case Tegra::Texture::SwizzleSource::A:
        return VK_COMPONENT_SWIZZLE_A;
    case Tegra::Texture::SwizzleSource::OneInt:
    case Tegra::Texture::SwizzleSource::OneFloat:
        return VK_COMPONENT_SWIZZLE_ONE;
    }
    UNIMPLEMENTED_MSG("Unimplemented swizzle source={}", swizzle);
    return {};
}

VkViewportCoordinateSwizzleNV ViewportSwizzle(Maxwell::ViewportSwizzle swizzle) {
    switch (swizzle) {
    case Maxwell::ViewportSwizzle::PositiveX:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV;
    case Maxwell::ViewportSwizzle::NegativeX:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_NEGATIVE_X_NV;
    case Maxwell::ViewportSwizzle::PositiveY:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV;
    case Maxwell::ViewportSwizzle::NegativeY:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_NEGATIVE_Y_NV;
    case Maxwell::ViewportSwizzle::PositiveZ:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV;
    case Maxwell::ViewportSwizzle::NegativeZ:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_NEGATIVE_Z_NV;
    case Maxwell::ViewportSwizzle::PositiveW:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV;
    case Maxwell::ViewportSwizzle::NegativeW:
        return VK_VIEWPORT_COORDINATE_SWIZZLE_NEGATIVE_W_NV;
    }
    ASSERT_MSG(false, "Invalid swizzle={}", swizzle);
    return {};
}

VkSamplerReductionMode SamplerReduction(Tegra::Texture::SamplerReduction reduction) {
    switch (reduction) {
    case Tegra::Texture::SamplerReduction::WeightedAverage:
        return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT;
    case Tegra::Texture::SamplerReduction::Min:
        return VK_SAMPLER_REDUCTION_MODE_MIN_EXT;
    case Tegra::Texture::SamplerReduction::Max:
        return VK_SAMPLER_REDUCTION_MODE_MAX_EXT;
    }
    ASSERT_MSG(false, "Invalid sampler mode={}", static_cast<int>(reduction));
    return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT;
}

VkSampleCountFlagBits MsaaMode(Tegra::Texture::MsaaMode msaa_mode) {
    switch (msaa_mode) {
    case Tegra::Texture::MsaaMode::Msaa1x1:
        return VK_SAMPLE_COUNT_1_BIT;
    case Tegra::Texture::MsaaMode::Msaa2x1:
    case Tegra::Texture::MsaaMode::Msaa2x1_D3D:
        return VK_SAMPLE_COUNT_2_BIT;
    case Tegra::Texture::MsaaMode::Msaa2x2:
    case Tegra::Texture::MsaaMode::Msaa2x2_VC4:
    case Tegra::Texture::MsaaMode::Msaa2x2_VC12:
        return VK_SAMPLE_COUNT_4_BIT;
    case Tegra::Texture::MsaaMode::Msaa4x2:
    case Tegra::Texture::MsaaMode::Msaa4x2_D3D:
    case Tegra::Texture::MsaaMode::Msaa4x2_VC8:
    case Tegra::Texture::MsaaMode::Msaa4x2_VC24:
        return VK_SAMPLE_COUNT_8_BIT;
    case Tegra::Texture::MsaaMode::Msaa4x4:
        return VK_SAMPLE_COUNT_16_BIT;
    default:
        ASSERT_MSG(false, "Invalid msaa_mode={}", static_cast<int>(msaa_mode));
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

} // namespace Vulkan::MaxwellToVK

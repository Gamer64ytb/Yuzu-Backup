// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <glad/glad.h>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"

namespace OpenGL::MaxwellToGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct FormatTuple {
    GLenum internal_format;
    GLenum format = GL_NONE;
    GLenum type = GL_NONE;
};

constexpr std::array<FormatTuple, VideoCore::Surface::MaxPixelFormat> FORMAT_TABLE = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},                 // A8B8G8R8_UNORM
    {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE},                               // A8B8G8R8_SNORM
    {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE},                            // A8B8G8R8_SINT
    {GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE},                  // A8B8G8R8_UINT
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},                     // R5G6B5_UNORM
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV},                 // B5G6R5_UNORM
    {GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},             // A1R5G5B5_UNORM
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},           // A2B10G10R10_UNORM
    {GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV}, // A2B10G10R10_UINT
    {GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV},           // A2R10G10B10_UNORM
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV},             // A1B5G5R5_UNORM
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1},                 // A5B5G5R1_UNORM
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE},                                // R8_UNORM
    {GL_R8_SNORM, GL_RED, GL_BYTE},                                   // R8_SNORM
    {GL_R8I, GL_RED_INTEGER, GL_BYTE},                                // R8_SINT
    {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE},                      // R8_UINT
    {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},                             // R16G16B16A16_FLOAT
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT},                          // R16G16B16A16_UNORM
    {GL_RGBA16_SNORM, GL_RGBA, GL_SHORT},                             // R16G16B16A16_SNORM
    {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT},                          // R16G16B16A16_SINT
    {GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT},                // R16G16B16A16_UINT
    {GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV},     // B10G11R11_FLOAT
    {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT},                  // R32G32B32A32_UINT
    {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT},                               // BC1_RGBA_UNORM
    {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT},                               // BC2_UNORM
    {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT},                               // BC3_UNORM
    {GL_COMPRESSED_RED_RGTC1},                                        // BC4_UNORM
    {GL_COMPRESSED_SIGNED_RED_RGTC1},                                 // BC4_SNORM
    {GL_COMPRESSED_RG_RGTC2},                                         // BC5_UNORM
    {GL_COMPRESSED_SIGNED_RG_RGTC2},                                  // BC5_SNORM
    {GL_COMPRESSED_RGBA_BPTC_UNORM},                                  // BC7_UNORM
    {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT},                          // BC6H_UFLOAT
    {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT},                            // BC6H_SFLOAT
    {GL_COMPRESSED_RGBA_ASTC_4x4_KHR},                                // ASTC_2D_4X4_UNORM
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},                 // B8G8R8A8_UNORM
    {GL_RGBA32F, GL_RGBA, GL_FLOAT},                                  // R32G32B32A32_FLOAT
    {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT},                            // R32G32B32A32_SINT
    {GL_RG32F, GL_RG, GL_FLOAT},                                      // R32G32_FLOAT
    {GL_RG32I, GL_RG_INTEGER, GL_INT},                                // R32G32_SINT
    {GL_R32F, GL_RED, GL_FLOAT},                                      // R32_FLOAT
    {GL_R16F, GL_RED, GL_HALF_FLOAT},                                 // R16_FLOAT
    {GL_R16, GL_RED, GL_UNSIGNED_SHORT},                              // R16_UNORM
    {GL_R16_SNORM, GL_RED, GL_SHORT},                                 // R16_SNORM
    {GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT},                    // R16_UINT
    {GL_R16I, GL_RED_INTEGER, GL_SHORT},                              // R16_SINT
    {GL_RG16, GL_RG, GL_UNSIGNED_SHORT},                              // R16G16_UNORM
    {GL_RG16F, GL_RG, GL_HALF_FLOAT},                                 // R16G16_FLOAT
    {GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT},                    // R16G16_UINT
    {GL_RG16I, GL_RG_INTEGER, GL_SHORT},                              // R16G16_SINT
    {GL_RG16_SNORM, GL_RG, GL_SHORT},                                 // R16G16_SNORM
    {GL_RGB32F, GL_RGB, GL_FLOAT},                                    // R32G32B32_FLOAT
    {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},          // A8B8G8R8_SRGB
    {GL_RG8, GL_RG, GL_UNSIGNED_BYTE},                                // R8G8_UNORM
    {GL_RG8_SNORM, GL_RG, GL_BYTE},                                   // R8G8_SNORM
    {GL_RG8I, GL_RG_INTEGER, GL_BYTE},                                // R8G8_SINT
    {GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE},                      // R8G8_UINT
    {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT},                      // R32G32_UINT
    {GL_RGB16F, GL_RGBA, GL_HALF_FLOAT},                              // R16G16B16X16_FLOAT
    {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT},                      // R32_UINT
    {GL_R32I, GL_RED_INTEGER, GL_INT},                                // R32_SINT
    {GL_COMPRESSED_RGBA_ASTC_8x8_KHR},                                // ASTC_2D_8X8_UNORM
    {GL_COMPRESSED_RGBA_ASTC_8x5_KHR},                                // ASTC_2D_8X5_UNORM
    {GL_COMPRESSED_RGBA_ASTC_5x4_KHR},                                // ASTC_2D_5X4_UNORM
    {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},          // B8G8R8A8_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT},                         // BC1_RGBA_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT},                         // BC2_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT},                         // BC3_SRGB
    {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM},                            // BC7_SRGB
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV},               // A4B4G4R4_UNORM
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE},                                // G4R4_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR},                        // ASTC_2D_4X4_SRGB
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR},                        // ASTC_2D_8X8_SRGB
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR},                        // ASTC_2D_8X5_SRGB
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR},                        // ASTC_2D_5X4_SRGB
    {GL_COMPRESSED_RGBA_ASTC_5x5_KHR},                                // ASTC_2D_5X5_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR},                        // ASTC_2D_5X5_SRGB
    {GL_COMPRESSED_RGBA_ASTC_10x8_KHR},                               // ASTC_2D_10X8_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR},                       // ASTC_2D_10X8_SRGB
    {GL_COMPRESSED_RGBA_ASTC_6x6_KHR},                                // ASTC_2D_6X6_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR},                        // ASTC_2D_6X6_SRGB
    {GL_COMPRESSED_RGBA_ASTC_10x6_KHR},                               // ASTC_2D_10X6_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR},                       // ASTC_2D_10X6_SRGB
    {GL_COMPRESSED_RGBA_ASTC_10x5_KHR},                               // ASTC_2D_10X5_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR},                       // ASTC_2D_10X5_SRGB
    {GL_COMPRESSED_RGBA_ASTC_10x10_KHR},                              // ASTC_2D_10X10_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR},                      // ASTC_2D_10X10_SRGB
    {GL_COMPRESSED_RGBA_ASTC_12x10_KHR},                              // ASTC_2D_12X10_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR},                      // ASTC_2D_12X10_SRGB
    {GL_COMPRESSED_RGBA_ASTC_12x12_KHR},                              // ASTC_2D_12X12_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR},                      // ASTC_2D_12X12_SRGB
    {GL_COMPRESSED_RGBA_ASTC_8x6_KHR},                                // ASTC_2D_8X6_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR},                        // ASTC_2D_8X6_SRGB
    {GL_COMPRESSED_RGBA_ASTC_6x5_KHR},                                // ASTC_2D_6X5_UNORM
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR},                        // ASTC_2D_6X5_SRGB
    {GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV},                // E5B9G9R9_FLOAT
    {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},            // D32_FLOAT
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT},    // D16_UNORM
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT_24_8}, // X8_D24_UNORM
    {GL_STENCIL_INDEX8, GL_STENCIL, GL_UNSIGNED_BYTE},                // S8_UINT
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},    // D24_UNORM_S8_UINT
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},    // S8_UINT_D24_UNORM
    {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL,
     GL_FLOAT_32_UNSIGNED_INT_24_8_REV}, // D32_FLOAT_S8_UINT
}};

inline const FormatTuple& GetFormatTuple(VideoCore::Surface::PixelFormat pixel_format) {
    ASSERT(static_cast<size_t>(pixel_format) < FORMAT_TABLE.size());
    return FORMAT_TABLE[static_cast<size_t>(pixel_format)];
}

inline GLenum VertexFormat(Maxwell::VertexAttribute attrib) {
    switch (attrib.type) {
    case Maxwell::VertexAttribute::Type::UnusedEnumDoNotUseBecauseItWillGoAway:
        ASSERT_MSG(false, "Invalid vertex attribute type!");
        break;
    case Maxwell::VertexAttribute::Type::UNorm:
    case Maxwell::VertexAttribute::Type::UScaled:
    case Maxwell::VertexAttribute::Type::UInt:
        switch (attrib.size) {
        case Maxwell::VertexAttribute::Size::Size_R8:
        case Maxwell::VertexAttribute::Size::Size_A8:
        case Maxwell::VertexAttribute::Size::Size_R8_G8:
        case Maxwell::VertexAttribute::Size::Size_G8_R8:
        case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
        case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
        case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
            return GL_UNSIGNED_BYTE;
        case Maxwell::VertexAttribute::Size::Size_R16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
            return GL_UNSIGNED_SHORT;
        case Maxwell::VertexAttribute::Size::Size_R32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32_B32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32_B32_A32:
            return GL_UNSIGNED_INT;
        case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
            return GL_UNSIGNED_INT_2_10_10_10_REV;
        default:
            break;
        }
        break;
    case Maxwell::VertexAttribute::Type::SNorm:
    case Maxwell::VertexAttribute::Type::SScaled:
    case Maxwell::VertexAttribute::Type::SInt:
        switch (attrib.size) {
        case Maxwell::VertexAttribute::Size::Size_R8:
        case Maxwell::VertexAttribute::Size::Size_A8:
        case Maxwell::VertexAttribute::Size::Size_R8_G8:
        case Maxwell::VertexAttribute::Size::Size_G8_R8:
        case Maxwell::VertexAttribute::Size::Size_R8_G8_B8:
        case Maxwell::VertexAttribute::Size::Size_R8_G8_B8_A8:
        case Maxwell::VertexAttribute::Size::Size_X8_B8_G8_R8:
            return GL_BYTE;
        case Maxwell::VertexAttribute::Size::Size_R16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
            return GL_SHORT;
        case Maxwell::VertexAttribute::Size::Size_R32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32_B32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32_B32_A32:
            return GL_INT;
        case Maxwell::VertexAttribute::Size::Size_A2_B10_G10_R10:
            return GL_INT_2_10_10_10_REV;
        default:
            break;
        }
        break;
    case Maxwell::VertexAttribute::Type::Float:
        switch (attrib.size) {
        case Maxwell::VertexAttribute::Size::Size_R16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16_B16:
        case Maxwell::VertexAttribute::Size::Size_R16_G16_B16_A16:
            return GL_HALF_FLOAT;
        case Maxwell::VertexAttribute::Size::Size_R32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32_B32:
        case Maxwell::VertexAttribute::Size::Size_R32_G32_B32_A32:
            return GL_FLOAT;
        case Maxwell::VertexAttribute::Size::Size_B10_G11_R11:
            return GL_UNSIGNED_INT_10F_11F_11F_REV;
        default:
            break;
        }
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented vertex format of type={} and size={}", attrib.TypeString(),
                      attrib.SizeString());
    return {};
}

inline GLenum IndexFormat(Maxwell::IndexFormat index_format) {
    switch (index_format) {
    case Maxwell::IndexFormat::UnsignedByte:
        return GL_UNSIGNED_BYTE;
    case Maxwell::IndexFormat::UnsignedShort:
        return GL_UNSIGNED_SHORT;
    case Maxwell::IndexFormat::UnsignedInt:
        return GL_UNSIGNED_INT;
    }
    ASSERT_MSG(false, "Invalid index_format={}", index_format);
    return {};
}

inline GLenum PrimitiveTopology(Maxwell::PrimitiveTopology topology) {
    switch (topology) {
    case Maxwell::PrimitiveTopology::Points:
        return GL_POINTS;
    case Maxwell::PrimitiveTopology::Lines:
        return GL_LINES;
    case Maxwell::PrimitiveTopology::LineLoop:
        return GL_LINE_LOOP;
    case Maxwell::PrimitiveTopology::LineStrip:
        return GL_LINE_STRIP;
    case Maxwell::PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    case Maxwell::PrimitiveTopology::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case Maxwell::PrimitiveTopology::TriangleFan:
        return GL_TRIANGLE_FAN;
    case Maxwell::PrimitiveTopology::Quads:
        return GL_QUADS;
    case Maxwell::PrimitiveTopology::QuadStrip:
        return GL_QUAD_STRIP;
    case Maxwell::PrimitiveTopology::Polygon:
        return GL_POLYGON;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
        return GL_LINES_ADJACENCY;
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        return GL_LINE_STRIP_ADJACENCY;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
        return GL_TRIANGLES_ADJACENCY;
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        return GL_TRIANGLE_STRIP_ADJACENCY;
    case Maxwell::PrimitiveTopology::Patches:
        return GL_PATCHES;
    }
    ASSERT_MSG(false, "Invalid topology={}", topology);
    return GL_POINTS;
}

inline GLenum TextureFilterMode(Tegra::Texture::TextureFilter filter_mode,
                                Tegra::Texture::TextureMipmapFilter mipmap_filter_mode) {
    switch (filter_mode) {
    case Tegra::Texture::TextureFilter::Nearest:
        switch (mipmap_filter_mode) {
        case Tegra::Texture::TextureMipmapFilter::None:
            return GL_NEAREST;
        case Tegra::Texture::TextureMipmapFilter::Nearest:
            return GL_NEAREST_MIPMAP_NEAREST;
        case Tegra::Texture::TextureMipmapFilter::Linear:
            return GL_NEAREST_MIPMAP_LINEAR;
        }
        break;
    case Tegra::Texture::TextureFilter::Linear:
        switch (mipmap_filter_mode) {
        case Tegra::Texture::TextureMipmapFilter::None:
            return GL_LINEAR;
        case Tegra::Texture::TextureMipmapFilter::Nearest:
            return GL_LINEAR_MIPMAP_NEAREST;
        case Tegra::Texture::TextureMipmapFilter::Linear:
            return GL_LINEAR_MIPMAP_LINEAR;
        }
        break;
    }
    ASSERT_MSG(false, "Invalid texture filter mode={} and mipmap filter mode={}", filter_mode,
               mipmap_filter_mode);
    return GL_NEAREST;
}

inline GLenum WrapMode(Tegra::Texture::WrapMode wrap_mode) {
    switch (wrap_mode) {
    case Tegra::Texture::WrapMode::Wrap:
        return GL_REPEAT;
    case Tegra::Texture::WrapMode::Mirror:
        return GL_MIRRORED_REPEAT;
    case Tegra::Texture::WrapMode::ClampToEdge:
        return GL_CLAMP_TO_EDGE;
    case Tegra::Texture::WrapMode::Border:
        return GL_CLAMP_TO_BORDER;
    case Tegra::Texture::WrapMode::Clamp:
        return GL_CLAMP;
    case Tegra::Texture::WrapMode::MirrorOnceClampToEdge:
        return GL_MIRROR_CLAMP_TO_EDGE;
    case Tegra::Texture::WrapMode::MirrorOnceBorder:
        if (GL_EXT_texture_mirror_clamp) {
            return GL_MIRROR_CLAMP_TO_BORDER_EXT;
        } else {
            return GL_MIRROR_CLAMP_TO_EDGE;
        }
    case Tegra::Texture::WrapMode::MirrorOnceClampOGL:
        if (GL_EXT_texture_mirror_clamp) {
            return GL_MIRROR_CLAMP_EXT;
        } else {
            return GL_MIRROR_CLAMP_TO_EDGE;
        }
    }
    UNIMPLEMENTED_MSG("Unimplemented texture wrap mode={}", wrap_mode);
    return GL_REPEAT;
}

inline GLenum DepthCompareFunc(Tegra::Texture::DepthCompareFunc func) {
    switch (func) {
    case Tegra::Texture::DepthCompareFunc::Never:
        return GL_NEVER;
    case Tegra::Texture::DepthCompareFunc::Less:
        return GL_LESS;
    case Tegra::Texture::DepthCompareFunc::LessEqual:
        return GL_LEQUAL;
    case Tegra::Texture::DepthCompareFunc::Equal:
        return GL_EQUAL;
    case Tegra::Texture::DepthCompareFunc::NotEqual:
        return GL_NOTEQUAL;
    case Tegra::Texture::DepthCompareFunc::Greater:
        return GL_GREATER;
    case Tegra::Texture::DepthCompareFunc::GreaterEqual:
        return GL_GEQUAL;
    case Tegra::Texture::DepthCompareFunc::Always:
        return GL_ALWAYS;
    }
    UNIMPLEMENTED_MSG("Unimplemented texture depth compare function={}", func);
    return GL_GREATER;
}

inline GLenum BlendEquation(Maxwell::Blend::Equation equation) {
    switch (equation) {
    case Maxwell::Blend::Equation::Add_D3D:
    case Maxwell::Blend::Equation::Add_GL:
        return GL_FUNC_ADD;
    case Maxwell::Blend::Equation::Subtract_D3D:
    case Maxwell::Blend::Equation::Subtract_GL:
        return GL_FUNC_SUBTRACT;
    case Maxwell::Blend::Equation::ReverseSubtract_D3D:
    case Maxwell::Blend::Equation::ReverseSubtract_GL:
        return GL_FUNC_REVERSE_SUBTRACT;
    case Maxwell::Blend::Equation::Min_D3D:
    case Maxwell::Blend::Equation::Min_GL:
        return GL_MIN;
    case Maxwell::Blend::Equation::Max_D3D:
    case Maxwell::Blend::Equation::Max_GL:
        return GL_MAX;
    }
    UNIMPLEMENTED_MSG("Unimplemented blend equation={}", equation);
    return GL_FUNC_ADD;
}

inline GLenum BlendFunc(Maxwell::Blend::Factor factor) {
    switch (factor) {
    case Maxwell::Blend::Factor::Zero_D3D:
    case Maxwell::Blend::Factor::Zero_GL:
        return GL_ZERO;
    case Maxwell::Blend::Factor::One_D3D:
    case Maxwell::Blend::Factor::One_GL:
        return GL_ONE;
    case Maxwell::Blend::Factor::SourceColor_D3D:
    case Maxwell::Blend::Factor::SourceColor_GL:
        return GL_SRC_COLOR;
    case Maxwell::Blend::Factor::OneMinusSourceColor_D3D:
    case Maxwell::Blend::Factor::OneMinusSourceColor_GL:
        return GL_ONE_MINUS_SRC_COLOR;
    case Maxwell::Blend::Factor::SourceAlpha_D3D:
    case Maxwell::Blend::Factor::SourceAlpha_GL:
        return GL_SRC_ALPHA;
    case Maxwell::Blend::Factor::OneMinusSourceAlpha_D3D:
    case Maxwell::Blend::Factor::OneMinusSourceAlpha_GL:
        return GL_ONE_MINUS_SRC_ALPHA;
    case Maxwell::Blend::Factor::DestAlpha_D3D:
    case Maxwell::Blend::Factor::DestAlpha_GL:
        return GL_DST_ALPHA;
    case Maxwell::Blend::Factor::OneMinusDestAlpha_D3D:
    case Maxwell::Blend::Factor::OneMinusDestAlpha_GL:
        return GL_ONE_MINUS_DST_ALPHA;
    case Maxwell::Blend::Factor::DestColor_D3D:
    case Maxwell::Blend::Factor::DestColor_GL:
        return GL_DST_COLOR;
    case Maxwell::Blend::Factor::OneMinusDestColor_D3D:
    case Maxwell::Blend::Factor::OneMinusDestColor_GL:
        return GL_ONE_MINUS_DST_COLOR;
    case Maxwell::Blend::Factor::SourceAlphaSaturate_D3D:
    case Maxwell::Blend::Factor::SourceAlphaSaturate_GL:
        return GL_SRC_ALPHA_SATURATE;
    case Maxwell::Blend::Factor::Source1Color_D3D:
    case Maxwell::Blend::Factor::Source1Color_GL:
        return GL_SRC1_COLOR;
    case Maxwell::Blend::Factor::OneMinusSource1Color_D3D:
    case Maxwell::Blend::Factor::OneMinusSource1Color_GL:
        return GL_ONE_MINUS_SRC1_COLOR;
    case Maxwell::Blend::Factor::Source1Alpha_D3D:
    case Maxwell::Blend::Factor::Source1Alpha_GL:
        return GL_SRC1_ALPHA;
    case Maxwell::Blend::Factor::OneMinusSource1Alpha_D3D:
    case Maxwell::Blend::Factor::OneMinusSource1Alpha_GL:
        return GL_ONE_MINUS_SRC1_ALPHA;
    case Maxwell::Blend::Factor::BlendFactor_D3D:
    case Maxwell::Blend::Factor::ConstantColor_GL:
        return GL_CONSTANT_COLOR;
    case Maxwell::Blend::Factor::OneMinusBlendFactor_D3D:
    case Maxwell::Blend::Factor::OneMinusConstantColor_GL:
        return GL_ONE_MINUS_CONSTANT_COLOR;
    case Maxwell::Blend::Factor::BothSourceAlpha_D3D:
    case Maxwell::Blend::Factor::ConstantAlpha_GL:
        return GL_CONSTANT_ALPHA;
    case Maxwell::Blend::Factor::OneMinusBothSourceAlpha_D3D:
    case Maxwell::Blend::Factor::OneMinusConstantAlpha_GL:
        return GL_ONE_MINUS_CONSTANT_ALPHA;
    }
    UNIMPLEMENTED_MSG("Unimplemented blend factor={}", factor);
    return GL_ZERO;
}

inline GLenum ComparisonOp(Maxwell::ComparisonOp comparison) {
    switch (comparison) {
    case Maxwell::ComparisonOp::Never_D3D:
    case Maxwell::ComparisonOp::Never_GL:
        return GL_NEVER;
    case Maxwell::ComparisonOp::Less_D3D:
    case Maxwell::ComparisonOp::Less_GL:
        return GL_LESS;
    case Maxwell::ComparisonOp::Equal_D3D:
    case Maxwell::ComparisonOp::Equal_GL:
        return GL_EQUAL;
    case Maxwell::ComparisonOp::LessEqual_D3D:
    case Maxwell::ComparisonOp::LessEqual_GL:
        return GL_LEQUAL;
    case Maxwell::ComparisonOp::Greater_D3D:
    case Maxwell::ComparisonOp::Greater_GL:
        return GL_GREATER;
    case Maxwell::ComparisonOp::NotEqual_D3D:
    case Maxwell::ComparisonOp::NotEqual_GL:
        return GL_NOTEQUAL;
    case Maxwell::ComparisonOp::GreaterEqual_D3D:
    case Maxwell::ComparisonOp::GreaterEqual_GL:
        return GL_GEQUAL;
    case Maxwell::ComparisonOp::Always_D3D:
    case Maxwell::ComparisonOp::Always_GL:
        return GL_ALWAYS;
    }
    UNIMPLEMENTED_MSG("Unimplemented comparison op={}", comparison);
    return GL_ALWAYS;
}

inline GLenum StencilOp(Maxwell::StencilOp::Op stencil) {
    switch (stencil) {
    case Maxwell::StencilOp::Op::Keep_D3D:
    case Maxwell::StencilOp::Op::Keep_GL:
        return GL_KEEP;
    case Maxwell::StencilOp::Op::Zero_D3D:
    case Maxwell::StencilOp::Op::Zero_GL:
        return GL_ZERO;
    case Maxwell::StencilOp::Op::Replace_D3D:
    case Maxwell::StencilOp::Op::Replace_GL:
        return GL_REPLACE;
    case Maxwell::StencilOp::Op::IncrSaturate_D3D:
    case Maxwell::StencilOp::Op::IncrSaturate_GL:
        return GL_INCR;
    case Maxwell::StencilOp::Op::DecrSaturate_D3D:
    case Maxwell::StencilOp::Op::DecrSaturate_GL:
        return GL_DECR;
    case Maxwell::StencilOp::Op::Invert_D3D:
    case Maxwell::StencilOp::Op::Invert_GL:
        return GL_INVERT;
    case Maxwell::StencilOp::Op::Incr_D3D:
    case Maxwell::StencilOp::Op::Incr_GL:
        return GL_INCR_WRAP;
    case Maxwell::StencilOp::Op::Decr_D3D:
    case Maxwell::StencilOp::Op::Decr_GL:
        return GL_DECR_WRAP;
    }
    UNIMPLEMENTED_MSG("Unimplemented stencil op={}", stencil);
    return GL_KEEP;
}

inline GLenum FrontFace(Maxwell::FrontFace front_face) {
    switch (front_face) {
    case Maxwell::FrontFace::ClockWise:
        return GL_CW;
    case Maxwell::FrontFace::CounterClockWise:
        return GL_CCW;
    }
    UNIMPLEMENTED_MSG("Unimplemented front face cull={}", front_face);
    return GL_CCW;
}

inline GLenum CullFace(Maxwell::CullFace cull_face) {
    switch (cull_face) {
    case Maxwell::CullFace::Front:
        return GL_FRONT;
    case Maxwell::CullFace::Back:
        return GL_BACK;
    case Maxwell::CullFace::FrontAndBack:
        return GL_FRONT_AND_BACK;
    }
    UNIMPLEMENTED_MSG("Unimplemented cull face={}", cull_face);
    return GL_BACK;
}

inline GLenum LogicOp(Maxwell::LogicOp::Op operation) {
    switch (operation) {
    case Maxwell::LogicOp::Op::Clear:
        return GL_CLEAR;
    case Maxwell::LogicOp::Op::And:
        return GL_AND;
    case Maxwell::LogicOp::Op::AndReverse:
        return GL_AND_REVERSE;
    case Maxwell::LogicOp::Op::Copy:
        return GL_COPY;
    case Maxwell::LogicOp::Op::AndInverted:
        return GL_AND_INVERTED;
    case Maxwell::LogicOp::Op::NoOp:
        return GL_NOOP;
    case Maxwell::LogicOp::Op::Xor:
        return GL_XOR;
    case Maxwell::LogicOp::Op::Or:
        return GL_OR;
    case Maxwell::LogicOp::Op::Nor:
        return GL_NOR;
    case Maxwell::LogicOp::Op::Equiv:
        return GL_EQUIV;
    case Maxwell::LogicOp::Op::Invert:
        return GL_INVERT;
    case Maxwell::LogicOp::Op::OrReverse:
        return GL_OR_REVERSE;
    case Maxwell::LogicOp::Op::CopyInverted:
        return GL_COPY_INVERTED;
    case Maxwell::LogicOp::Op::OrInverted:
        return GL_OR_INVERTED;
    case Maxwell::LogicOp::Op::Nand:
        return GL_NAND;
    case Maxwell::LogicOp::Op::Set:
        return GL_SET;
    }
    UNIMPLEMENTED_MSG("Unimplemented logic operation={}", operation);
    return GL_COPY;
}

inline GLenum PolygonMode(Maxwell::PolygonMode polygon_mode) {
    switch (polygon_mode) {
    case Maxwell::PolygonMode::Point:
        return GL_POINT;
    case Maxwell::PolygonMode::Line:
        return GL_LINE;
    case Maxwell::PolygonMode::Fill:
        return GL_FILL;
    }
    ASSERT_MSG(false, "Invalid polygon mode={}", polygon_mode);
    return GL_FILL;
}

inline GLenum ReductionFilter(Tegra::Texture::SamplerReduction filter) {
    switch (filter) {
    case Tegra::Texture::SamplerReduction::WeightedAverage:
        return GL_WEIGHTED_AVERAGE_ARB;
    case Tegra::Texture::SamplerReduction::Min:
        return GL_MIN;
    case Tegra::Texture::SamplerReduction::Max:
        return GL_MAX;
    }
    ASSERT_MSG(false, "Invalid reduction filter={}", static_cast<int>(filter));
    return GL_WEIGHTED_AVERAGE_ARB;
}

inline GLenum ViewportSwizzle(Maxwell::ViewportSwizzle swizzle) {
    // Enumeration order matches register order. We can convert it arithmetically.
    return GL_VIEWPORT_SWIZZLE_POSITIVE_X_NV + static_cast<GLenum>(swizzle);
}

} // namespace OpenGL::MaxwellToGL

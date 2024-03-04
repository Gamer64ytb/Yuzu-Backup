// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/texture_cache/types.h"

namespace OpenGL {

class Image;
class ProgramManager;

struct StagingBufferMap;

class UtilShaders {
public:
    explicit UtilShaders(ProgramManager& program_manager);
    ~UtilShaders();

    void ASTCDecode(Image& image, const StagingBufferMap& map,
                    std::span<const VideoCommon::SwizzleParameters> swizzles);

    void BlockLinearUpload2D(Image& image, const StagingBufferMap& map,
                             std::span<const VideoCommon::SwizzleParameters> swizzles);

    void BlockLinearUpload3D(Image& image, const StagingBufferMap& map,
                             std::span<const VideoCommon::SwizzleParameters> swizzles);

    void PitchUpload(Image& image, const StagingBufferMap& map,
                     std::span<const VideoCommon::SwizzleParameters> swizzles);

    void CopyBC4(Image& dst_image, Image& src_image,
                 std::span<const VideoCommon::ImageCopy> copies);

    void ConvertS8D24(Image& dst_image, std::span<const VideoCommon::ImageCopy> copies);

    void CopyMSAA(Image& dst_image, Image& src_image,
                  std::span<const VideoCommon::ImageCopy> copies);

private:
    ProgramManager& program_manager;

    OGLBuffer swizzle_table_buffer;

    OGLProgram astc_decoder_program;
    OGLProgram block_linear_unswizzle_2d_program;
    OGLProgram block_linear_unswizzle_3d_program;
    OGLProgram pitch_unswizzle_program;
    OGLProgram copy_bc4_program;
    OGLProgram convert_s8d24_program;
    OGLProgram convert_ms_to_nonms_program;
    OGLProgram convert_nonms_to_ms_program;
};

GLenum StoreFormat(u32 bytes_per_block);

} // namespace OpenGL

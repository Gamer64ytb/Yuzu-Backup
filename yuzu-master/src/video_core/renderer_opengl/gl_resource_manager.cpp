// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string_view>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/microprofile.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

MICROPROFILE_DEFINE(OpenGL_ResourceCreation, "OpenGL", "Resource Creation", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_ResourceDeletion, "OpenGL", "Resource Deletion", MP_RGB(128, 128, 192));

namespace OpenGL {

void OGLRenderbuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateRenderbuffers(1, &handle);
}

void OGLRenderbuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteRenderbuffers(1, &handle);
    handle = 0;
}

void OGLTexture::Create(GLenum target) {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateTextures(target, 1, &handle);
}

void OGLTexture::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteTextures(1, &handle);
    handle = 0;
}

void OGLTextureView::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenTextures(1, &handle);
}

void OGLTextureView::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteTextures(1, &handle);
    handle = 0;
}

void OGLSampler::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateSamplers(1, &handle);
}

void OGLSampler::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteSamplers(1, &handle);
    handle = 0;
}

void OGLShader::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteShader(handle);
    handle = 0;
}

void OGLProgram::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgram(handle);
    handle = 0;
}

void OGLAssemblyProgram::Release() {
    if (handle == 0) {
        return;
    }
    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgramsARB(1, &handle);
    handle = 0;
}

void OGLPipeline::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenProgramPipelines(1, &handle);
}

void OGLPipeline::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgramPipelines(1, &handle);
    handle = 0;
}

void OGLBuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateBuffers(1, &handle);
}

void OGLBuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteBuffers(1, &handle);
    handle = 0;
}

void OGLSync::Create() {
    if (handle != 0)
        return;

    // Don't profile here, this one is expected to happen ingame.
    handle = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void OGLSync::Release() {
    if (handle == 0)
        return;

    // Don't profile here, this one is expected to happen ingame.
    glDeleteSync(handle);
    handle = 0;
}

bool OGLSync::IsSignaled() const noexcept {
    // At least on Nvidia, glClientWaitSync with a timeout of 0
    // is faster than glGetSynciv of GL_SYNC_STATUS.
    // Timeout of 0 means this check is non-blocking.
    const auto sync_status = glClientWaitSync(handle, 0, 0);
    ASSERT(sync_status != GL_WAIT_FAILED);
    return sync_status != GL_TIMEOUT_EXPIRED;
}

void OGLFramebuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    // Bind to READ_FRAMEBUFFER to stop Nvidia's driver from creating an EXT_framebuffer instead of
    // a core framebuffer. EXT framebuffer attachments have to match in size and can be shared
    // across contexts. yuzu doesn't share framebuffers across contexts and we need attachments with
    // mismatching size, this is why core framebuffers are preferred.
    glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, handle);
}

void OGLFramebuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteFramebuffers(1, &handle);
    handle = 0;
}

void OGLQuery::Create(GLenum target) {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateQueries(target, 1, &handle);
}

void OGLQuery::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteQueries(1, &handle);
    handle = 0;
}

void OGLTransformFeedback::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateTransformFeedbacks(1, &handle);
}

void OGLTransformFeedback::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteTransformFeedbacks(1, &handle);
    handle = 0;
}

} // namespace OpenGL

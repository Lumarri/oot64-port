// SPDX-License-Identifier: GPL-3.0-or-later
#include "gles_render_target.h"
#include <utility>

namespace Mm3d::Android {
void GlesRenderTarget::Release() {
    if (framebuffer_) glDeleteFramebuffers(1, &framebuffer_);
    if (color_) glDeleteTextures(1, &color_);
    if (depthStencil_) glDeleteRenderbuffers(1, &depthStencil_);
    AbandonContext();
}

void GlesRenderTarget::AbandonContext() {
    framebuffer_ = color_ = depthStencil_ = 0;
    width_ = height_ = 0;
}

bool GlesRenderTarget::Resize(GLsizei width, GLsizei height, std::string* error) {
    if (error) error->clear();
    const auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (width <= 0 || height <= 0) return fail("Render target dimensions must be positive");
    if (framebuffer_ && width == width_ && height == height_) return true;
    GLint textureLimit = 0, renderbufferLimit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureLimit);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbufferLimit);
    if (width > textureLimit || height > textureLimit ||
        width > renderbufferLimit || height > renderbufferLimit)
        return fail("Render target dimensions exceed GLES limits");
    // Callers must enter with a clean GL error state.
    if (glGetError() != GL_NO_ERROR) return fail("Pre-existing GLES error before target allocation");
    GLint draw = 0, read = 0, texture = 0, renderbuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
    GlesRenderTarget replacement;
    glGenTextures(1, &replacement.color_);
    glBindTexture(GL_TEXTURE_2D, replacement.color_);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &replacement.depthStencil_);
    glBindRenderbuffer(GL_RENDERBUFFER, replacement.depthStencil_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glGenFramebuffers(1, &replacement.framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, replacement.framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, replacement.color_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, replacement.depthStencil_);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &attachment);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    const GLenum allocationError = glGetError();
    const bool success = complete && allocationError == GL_NO_ERROR;
    const auto remap = [success](GLint binding, GLuint oldObject, GLuint newObject) -> GLuint {
        return success && oldObject != 0 && static_cast<GLuint>(binding) == oldObject
            ? newObject : static_cast<GLuint>(binding);
    };
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, remap(draw, framebuffer_, replacement.framebuffer_));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, remap(read, framebuffer_, replacement.framebuffer_));
    glBindTexture(GL_TEXTURE_2D, remap(texture, color_, replacement.color_));
    glBindRenderbuffer(GL_RENDERBUFFER, remap(renderbuffer, depthStencil_, replacement.depthStencil_));
    if (!success) return fail("Could not allocate a complete RGBA8/depth24/stencil8 target");
    std::swap(framebuffer_, replacement.framebuffer_);
    std::swap(color_, replacement.color_);
    std::swap(depthStencil_, replacement.depthStencil_);
    width_ = width;
    height_ = height;
    return true;
}
}

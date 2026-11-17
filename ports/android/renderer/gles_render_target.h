// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <GLES3/gl32.h>
#include <string>

namespace Mm3d::Android {
// Owned RGBA8 + depth24/stencil8 offscreen target, without MSAA.
// All calls, including destruction, require the owning GLES context current.
// Release before destroying that context. This is not a guest-memory cache.
class GlesRenderTarget final {
public:
    GlesRenderTarget() = default;
    ~GlesRenderTarget() { Release(); }
    GlesRenderTarget(const GlesRenderTarget&) = delete;
    GlesRenderTarget& operator=(const GlesRenderTarget&) = delete;

    // Same dimensions preserve content. Failure preserves the previous target.
    // Success replaces content (undefined until cleared), remapping read/draw
    // FBO, renderbuffer and active texture-unit bindings that referenced old
    // objects. Consumers on other texture units must rebind Color() afterwards.
    // The caller owns viewport state and must enter without pending GL errors.
    bool Resize(GLsizei width, GLsizei height, std::string* error = nullptr);
    void Release();
    // Only after the owning context is lost/destroyed; makes no GL calls.
    void AbandonContext();
    GLuint Framebuffer() const { return framebuffer_; }
    GLuint Color() const { return color_; }
    GLsizei Width() const { return width_; }
    GLsizei Height() const { return height_; }

private:
    GLuint framebuffer_ = 0, color_ = 0, depthStencil_ = 0;
    GLsizei width_ = 0, height_ = 0;
};
}

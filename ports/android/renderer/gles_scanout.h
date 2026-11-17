// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gles_render_target.h"

namespace Mm3d::Android {
// Portrait CTR transfer image -> logical landscape, fitted with black borders.
// Owns GL state during Present. Caller owns the destination FBO/surface and swap.
// Calls/destruction require this object's GLES context current.
class GlesScanout final {
public:
    GlesScanout() = default;
    ~GlesScanout() { Release(); }
    GlesScanout(const GlesScanout&) = delete;
    GlesScanout& operator=(const GlesScanout&) = delete;
    bool Present(const GlesRenderTarget& image, GLuint destination, GLsizei width, GLsizei height,
                 bool flip, bool overlay, std::string* error = nullptr);
    void Release();
    void AbandonContext(); // Only after context loss; makes no GL calls.
private:
    GLuint program_=0, vao_=0;
};
}

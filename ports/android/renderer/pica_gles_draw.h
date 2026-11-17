// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "gles_render_target.h"
#include "fast/renderer3ds/pica_render_backend.h"
#include <array>
#include <map>
#include <string>
#include <tuple>

namespace Mm3d::Android {
// Initial synchronous consumer of the upstream draw contract. Caller supplies
// an owned target and orders/clears it. No guest memory aliasing or presentation.
// Supports streamed vertex/index buffers, UBO bindings 0/4, raster state and
// RGBA8 color logic and streamed ordinary 2D textures at mip level zero.
// Shadow textures, GPU copy references and geometry shaders fail closed.
// Owns GL state during Submit; destruction/Release require its context current.
class PicaGlesDraw final {
public:
    PicaGlesDraw() = default;
    ~PicaGlesDraw() { Release(); }
    PicaGlesDraw(const PicaGlesDraw&) = delete;
    PicaGlesDraw& operator=(const PicaGlesDraw&) = delete;
    bool Submit(const Fast::Renderer3ds::PicaDrawView& draw, GlesRenderTarget& target,
                std::string* error = nullptr);
    void Release();
    void AbandonContext(); // Only after context loss; forget handles without GL calls.
    size_t CachedPrograms() const { return programs_.size(); }
private:
    using ProgramKey = std::tuple<std::string,std::string,bool>;
    std::map<ProgramKey,GLuint> programs_;
    GLuint vao_ = 0, indices_ = 0;
    std::array<GLuint,16> vertices_{};
    std::array<GLuint,2> uniforms_{};
    std::array<GLuint,3> textures_{};
};
}

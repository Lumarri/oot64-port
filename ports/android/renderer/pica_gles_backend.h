// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pica_gles_draw.h"
#include "gles_scanout.h"
#include <memory>
#include <optional>

namespace Mm3d::Android {
// Initial runtime-facing backend. Owns GLES state and requires its context
// current for calls/destruction. Guest physical ranges identify targets; they
// are not host pointers. Overlapping targets/textures are rejected until a
// guest/GPU coherence implementation exists. Host supplies the presentation FBO
// and owns EGL window/swap; no Android Activity is created here.
class PicaGlesBackend final : public Fast::Renderer3ds::PicaRenderBackend {
public:
    bool SubmitPicaDraw(const Fast::Renderer3ds::PicaDrawView& draw,
                        std::string* error = nullptr) override;
    bool SubmitPicaDisplayTransfer(const Fast::Renderer3ds::PicaDisplayTransferView&,
                                   std::string* error = nullptr) override;
    bool ClearPicaRenderTarget(uint64_t targetNamespace, uint32_t colorAddress,
                               std::string* error = nullptr) override;
    // Mirrors 32-bit GX fills exactly matching one whole attachment on the GPU.
    // Guest RAM updates and
    // interrupt publication remain with the caller, as in the upstream GL path.
    bool SubmitPicaMemoryFill(const Fast::Renderer3ds::PicaMemoryFillView& fill,
                              std::string* error = nullptr) override;
    bool QueuePicaCompletion(uint64_t id, std::string* error = nullptr) override;
    std::vector<uint64_t> TakePicaCompletions() override;
    bool ResetPicaState(std::string* error = nullptr) override;
    void AbandonContext(); // Only after context loss; discard all context-owned state without GL calls.

    // Borrowed until Reset/destruction. Never creates a target or changes state.
    const GlesRenderTarget* FindTarget(uint64_t targetNamespace, uint32_t colorAddress) const;
    const GlesRenderTarget* FindDisplay(uint64_t targetNamespace, uint32_t outputAddress) const;
    size_t TargetCount() const { return targets_.size(); }
    size_t DisplayCount() const { return displays_.size(); }
    // Destination is borrowed; use FBO 0 for a host EGL window. Reconfigure on
    // size/surface changes and DisablePresentation before destroying it.
    bool ConfigurePresentation(GLuint framebuffer, GLsizei width, GLsizei height, std::string* error = nullptr);
    void DisablePresentation() { presentation_.reset(); }
private:
    struct Target {
        uint64_t Namespace = 0;
        uint32_t ColorAddress = 0, DepthAddress = 0;
        std::unique_ptr<GlesRenderTarget> Image;
    };
    std::vector<Target> targets_;
    struct Display {
        uint64_t Namespace;
        uint32_t Address;
        std::unique_ptr<GlesRenderTarget> Image;
    };
    struct Presentation { GLuint Framebuffer; GLsizei Width, Height; };
    std::vector<Display> displays_;
    std::optional<Presentation> presentation_;
    std::vector<Fast::Renderer3ds::PicaMemoryFillView> pendingFills_;
    std::vector<uint64_t> completions_;
    PicaGlesDraw draws_;
    GlesScanout scanout_;
};
}

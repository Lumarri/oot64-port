// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_backend.h"
#include <algorithm>
#include <stdexcept>

namespace Mm3d::Android {
namespace {
using namespace Fast::Renderer3ds;
void Check(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
template<class F> bool Attempt(std::string* error, F&& action) {
    if (error) error->clear();
    try { action(); return true; }
    catch (const std::exception& failure) { if (error) *error=failure.what(); return false; }
}
struct Range { uint64_t Start, End; };
Range MemoryRange(uint32_t address, uint64_t size) {
    Check(address && size && uint64_t(address)+size<=0x100000000ULL,"Invalid PICA physical range");
    return {address,uint64_t(address)+size};
}
bool Overlap(Range a, Range b) { return a.Start<b.End && b.Start<a.End; }
bool Covers(Range a, Range b) { return a.Start<=b.Start && a.End>=b.End; }
Range ImageRange(uint32_t address, const GlesRenderTarget& image) {
    return MemoryRange(address,uint64_t(image.Width())*image.Height()*4);
}
void Clear(const GlesRenderTarget& image, bool color, bool depth, uint32_t value=0) {
    glBindFramebuffer(GL_FRAMEBUFFER,image.Framebuffer());
    glDisable(GL_SCISSOR_TEST);
    GLbitfield mask=0;
    if (color) {
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        // Guest RGBA8 is stored as bytes A,B,G,R.
        glClearColor(float((value>>24)&255)/255,float((value>>16)&255)/255,
                     float((value>>8)&255)/255,float(value&255)/255);
        mask|=GL_COLOR_BUFFER_BIT;
    }
    if (depth) {
        glDepthMask(GL_TRUE); glStencilMask(255);
        glClearDepthf(float(value&0xFFFFFF)/16777215.f); glClearStencil(value>>24);
        mask|=GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT;
    }
    glClear(mask);
    Check(glGetError()==GL_NO_ERROR,"GLES error clearing PICA target");
}
// Refuse partial writes rather than silently preserving stale GPU contents.
void ValidateFill(Range fill, Range color, Range depth) {
    Check((!Overlap(fill,color)||Covers(fill,color)) && (!Overlap(fill,depth)||Covers(fill,depth)),
          "Partial PICA target fills require guest/GPU coherence support");
    if (Overlap(fill,color)||Overlap(fill,depth))
        Check((fill.Start==color.Start && fill.End==color.End) ||
              (fill.Start==depth.Start && fill.End==depth.End),
              "Initial PICA fills must exactly match one target attachment");
}
}

const GlesRenderTarget* PicaGlesBackend::FindTarget(uint64_t ns, uint32_t address) const {
    for (const auto& target : targets_)
        if (target.Namespace==ns && target.ColorAddress==address) return target.Image.get();
    return nullptr;
}

const GlesRenderTarget* PicaGlesBackend::FindDisplay(uint64_t ns,uint32_t address) const {
    for (const auto& display : displays_)
        if (display.Namespace==ns && display.Address==address) return display.Image.get();
    return nullptr;
}

bool PicaGlesBackend::ConfigurePresentation(GLuint framebuffer,GLsizei width,GLsizei height,std::string* error) {
    return Attempt(error,[&] {
        GLint limits[2]{}; glGetIntegerv(GL_MAX_VIEWPORT_DIMS,limits);
        Check(width>0 && height>0 && width<=limits[0] && height<=limits[1],"Invalid presentation extent");
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before presentation configuration");
        Check(framebuffer==0 || glIsFramebuffer(framebuffer),"Invalid presentation framebuffer");
        for (const auto& target : targets_) Check(framebuffer!=target.Image->Framebuffer(),"Presentation cannot overwrite a PICA target");
        for (const auto& display : displays_) Check(framebuffer!=display.Image->Framebuffer(),"Presentation cannot overwrite a transfer image");
        GLint previous=0; glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previous);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer);
        const auto status=glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous);
        Check(status==GL_FRAMEBUFFER_COMPLETE && glGetError()==GL_NO_ERROR,"Incomplete presentation framebuffer");
        presentation_=Presentation{framebuffer,width,height};
    });
}

bool PicaGlesBackend::SubmitPicaDraw(const PicaDrawView& draw, std::string* error) {
    return Attempt(error,[&] {
        Check(draw.FramebufferColorFormat==0 && draw.FramebufferDepthFormat==3,
              "GLES backend requires RGBA8 and depth24/stencil8");
        Check(draw.FramebufferColorPhysicalAddress%4==0 && draw.FramebufferDepthPhysicalAddress%4==0,
              "Unaligned PICA target physical address");
        const uint64_t bytes=uint64_t(draw.FramebufferWidth)*draw.FramebufferHeight*4;
        const auto color=MemoryRange(draw.FramebufferColorPhysicalAddress,bytes);
        const auto depth=MemoryRange(draw.FramebufferDepthPhysicalAddress,bytes);
        Check(!Overlap(color,depth),"PICA color and depth ranges overlap");
        Target* selected=nullptr;
        for (auto& target : targets_) {
            if (target.Namespace!=draw.RenderTargetNamespace) continue;
            const auto oldColor=ImageRange(target.ColorAddress,*target.Image);
            const auto oldDepth=ImageRange(target.DepthAddress,*target.Image);
            if (target.ColorAddress==draw.FramebufferColorPhysicalAddress &&
                target.DepthAddress==draw.FramebufferDepthPhysicalAddress &&
                target.Image->Width()==draw.FramebufferWidth && target.Image->Height()==draw.FramebufferHeight)
                selected=&target;
            else Check(!Overlap(color,oldColor)&&!Overlap(color,oldDepth)&&
                       !Overlap(depth,oldColor)&&!Overlap(depth,oldDepth),
                       "Aliased PICA targets require guest/GPU coherence support");
        }
        for (const auto& display : displays_) if (display.Namespace==draw.RenderTargetNamespace) {
            const auto range=ImageRange(display.Address,*display.Image);
            Check(!Overlap(color,range)&&!Overlap(depth,range),"PICA target aliases a display image");
        }
        for (const auto& texture : draw.Textures) {
            if (texture.NativeBytes.empty()) continue; // The draw validator reports missing bytes.
            const auto sampled=MemoryRange(texture.PhysicalAddress,texture.NativeBytes.size());
            Check(!Overlap(sampled,color)&&!Overlap(sampled,depth),"PICA feedback textures are not yet supported");
            for (const auto& target : targets_) if (target.Namespace==draw.RenderTargetNamespace)
                Check(!Overlap(sampled,ImageRange(target.ColorAddress,*target.Image)) &&
                      !Overlap(sampled,ImageRange(target.DepthAddress,*target.Image)),
                      "PICA target textures require guest/GPU coherence support");
            for (const auto& display : displays_) if (display.Namespace==draw.RenderTargetNamespace)
                Check(!Overlap(sampled,ImageRange(display.Address,*display.Image)),
                      "Display textures require guest/GPU coherence support");
        }
        for (const auto& fill : pendingFills_) if (fill.RenderTargetNamespace==draw.RenderTargetNamespace)
            ValidateFill({fill.StartPhysicalAddress,fill.EndPhysicalAddress},color,depth);
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before backend draw");
        bool created=false;
        if (!selected) {
            Check(targets_.size()<64,"Initial PICA target limit reached");
            auto image=std::make_unique<GlesRenderTarget>();
            std::string detail;
            if (!image->Resize(draw.FramebufferWidth,draw.FramebufferHeight,&detail)) throw std::runtime_error(detail);
            Clear(*image,true,false); Clear(*image,false,true,0xFFFFFF);
            targets_.push_back({draw.RenderTargetNamespace,draw.FramebufferColorPhysicalAddress,
                                draw.FramebufferDepthPhysicalAddress,std::move(image)});
            selected=&targets_.back(); created=true;
        }
        try {
            // A pending fill belongs to the first later target that consumes it.
            // Preserve it across a rejected draw so retrying observes the same state.
            for (const auto& fill : pendingFills_) if (fill.RenderTargetNamespace==selected->Namespace) {
                const Range range{fill.StartPhysicalAddress,fill.EndPhysicalAddress};
                if (Overlap(range,color)||Overlap(range,depth))
                    Clear(*selected->Image,Covers(range,color),Covers(range,depth),fill.Value);
            }
            std::string detail;
            if (!draws_.Submit(draw,*selected->Image,&detail)) throw std::runtime_error(detail);
        } catch (...) {
            if (created) targets_.pop_back();
            throw;
        }
        std::erase_if(pendingFills_,[&](const auto& fill) {
            const Range range{fill.StartPhysicalAddress,fill.EndPhysicalAddress};
            return fill.RenderTargetNamespace==draw.RenderTargetNamespace && (Overlap(range,color)||Overlap(range,depth));
        });
    });
}

bool PicaGlesBackend::ClearPicaRenderTarget(uint64_t ns,uint32_t address,std::string* error) {
    return Attempt(error,[&] {
        Check(address!=0,"Invalid PICA target clear address");
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before target clear");
        if (const auto* image=FindTarget(ns,address)) {
            Clear(*image,true,false); Clear(*image,false,true,0xFFFFFF);
        }
    });
}

bool PicaGlesBackend::SubmitPicaMemoryFill(const PicaMemoryFillView& fill,std::string* error) {
    return Attempt(error,[&] {
        Check(fill.StartPhysicalAddress && fill.EndPhysicalAddress>fill.StartPhysicalAddress &&
              fill.StartPhysicalAddress%4==0 && fill.EndPhysicalAddress%4==0 &&
              (fill.Control&0x201)==0x201 && (fill.Control&~0x203)==0,
              "Initial GLES fills require enabled, aligned 32-bit writes");
        const Range range{fill.StartPhysicalAddress,fill.EndPhysicalAddress};
        for (const auto& display : displays_) if (display.Namespace==fill.RenderTargetNamespace)
            Check(!Overlap(range,ImageRange(display.Address,*display.Image)),"PICA fill aliases a display image");
        bool matched=false;
        for (const auto& target : targets_) if (target.Namespace==fill.RenderTargetNamespace) {
            const auto color=ImageRange(target.ColorAddress,*target.Image);
            const auto depth=ImageRange(target.DepthAddress,*target.Image);
            ValidateFill(range,color,depth); matched|=Overlap(range,color)||Overlap(range,depth);
        }
        // Ordered overlapping pending fills need byte-range splitting. Refuse them
        // until that exists; exact range replacement is safe and keeps the latest.
        auto previous=pendingFills_.end();
        for (auto it=pendingFills_.begin();it!=pendingFills_.end();++it) {
            if (it->RenderTargetNamespace!=fill.RenderTargetNamespace) continue;
            const Range old{it->StartPhysicalAddress,it->EndPhysicalAddress};
            if (old.Start==range.Start && old.End==range.End) previous=it;
            else Check(!Overlap(range,old),"Overlapping pending PICA fills are not yet supported");
        }
        if (!matched && previous==pendingFills_.end()) Check(pendingFills_.size()<128,"Pending PICA fill limit reached");
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before PICA fill");
        for (const auto& target : targets_) if (target.Namespace==fill.RenderTargetNamespace) {
            const auto color=ImageRange(target.ColorAddress,*target.Image);
            const auto depth=ImageRange(target.DepthAddress,*target.Image);
            if (Overlap(range,color)||Overlap(range,depth)) Clear(*target.Image,Covers(range,color),Covers(range,depth),fill.Value);
        }
        if (matched) { if (previous!=pendingFills_.end()) pendingFills_.erase(previous); }
        else if (previous!=pendingFills_.end()) *previous=fill;
        else pendingFills_.push_back(fill);
    });
}

bool PicaGlesBackend::SubmitPicaDisplayTransfer(const PicaDisplayTransferView& transfer,std::string* error) {
    return Attempt(error,[&] {
        Check(!transfer.MaterializeGuest && !transfer.RenderTargetCapture && transfer.TextureCopyBytes==0,
              "Guest materialization, producer captures and raw texture copies are not yet supported");
        // RGBA8 -> RGBA8, linear scanout output, optional flip, 1x/2x/2x2 scaling.
        // Other format/mode/reserved bits must never be silently ignored.
        Check((transfer.Flags&~0x03000023U)==0,"Unsupported PICA display transfer flags or format");
        const unsigned scaling=(transfer.Flags>>24)&3;
        Check(scaling<=2,"Unsupported PICA display transfer scaling");
        const bool inputLinear=(transfer.Flags&2)!=0, dontSwizzle=(transfer.Flags&32)!=0;
        Check(inputLinear==dontSwizzle,"Tiled display output requires GPU producer support");
        Check(transfer.PresentationMode==PicaPresentationMode::Replace ||
              transfer.PresentationMode==PicaPresentationMode::AlphaOverlay,"Invalid presentation mode");
        Check(!transfer.Present || presentation_.has_value(),"Host has not configured a presentation surface");
        const auto* source=FindTarget(transfer.RenderTargetNamespace,transfer.InputPhysicalAddress);
        Check(source!=nullptr,"PICA display transfer source is missing");
        Check(transfer.InputWidth==source->Width() && transfer.InputHeight==source->Height(),
              "Initial display transfer requires the complete source extent");
        const unsigned horizontalSamples=scaling?2:1, verticalSamples=scaling==2?2:1;
        Check(transfer.OutputWidth>0 && transfer.OutputHeight>0 &&
              unsigned(transfer.OutputWidth)*horizontalSamples==transfer.InputWidth &&
              unsigned(transfer.OutputHeight)*verticalSamples==transfer.InputHeight,
              "Display transfer dimensions do not match its scaling mode");
        Check(transfer.OutputPhysicalAddress%4==0,"Unaligned display output address");
        const auto output=MemoryRange(transfer.OutputPhysicalAddress,
                                     uint64_t(transfer.OutputWidth)*transfer.OutputHeight*4);
        for (const auto& target : targets_) if (target.Namespace==transfer.RenderTargetNamespace)
            Check(!Overlap(output,ImageRange(target.ColorAddress,*target.Image)) &&
                  !Overlap(output,ImageRange(target.DepthAddress,*target.Image)),"Display output aliases a PICA target");
        auto previous=displays_.end();
        for (auto it=displays_.begin();it!=displays_.end();++it) if (it->Namespace==transfer.RenderTargetNamespace) {
            const auto range=ImageRange(it->Address,*it->Image);
            if (it->Address==transfer.OutputPhysicalAddress) previous=it;
            else Check(!Overlap(output,range),"Aliased PICA display outputs are not yet supported");
        }
        for (const auto& fill : pendingFills_) if (fill.RenderTargetNamespace==transfer.RenderTargetNamespace)
            Check(!Overlap(output,{fill.StartPhysicalAddress,fill.EndPhysicalAddress}),"Display output overlaps a pending fill");
        Check(previous!=displays_.end() || displays_.size()<64,"Initial display image limit reached");
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before display transfer");
        auto image=std::make_unique<GlesRenderTarget>();
        std::string detail;
        if (!image->Resize(transfer.OutputWidth,transfer.OutputHeight,&detail)) throw std::runtime_error(detail);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,source->Framebuffer());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,image->Framebuffer());
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(0,0,transfer.InputWidth,transfer.InputHeight,0,0,transfer.OutputWidth,transfer.OutputHeight,
                          GL_COLOR_BUFFER_BIT,scaling?GL_LINEAR:GL_NEAREST);
        Check(glGetError()==GL_NO_ERROR,"GLES error copying display transfer");
        if (transfer.Present) {
            const bool flip=((!inputLinear!=(inputLinear!=dontSwizzle))^((transfer.Flags&1)!=0));
            if (!scanout_.Present(*image,presentation_->Framebuffer,presentation_->Width,presentation_->Height,
                                  flip,transfer.PresentationMode==PicaPresentationMode::AlphaOverlay,&detail))
                throw std::runtime_error(detail);
        }
        // Publish an owned snapshot only after copy/presentation succeeds. Later
        // draws to the source cannot alter a display already transferred.
        if (previous==displays_.end()) displays_.push_back({transfer.RenderTargetNamespace,transfer.OutputPhysicalAddress,std::move(image)});
        else previous->Image=std::move(image);
        // Completion/guest writes are not invented here. The runtime queues its
        // completion after submission; MaterializeGuest requests fail above.
    });
}
bool PicaGlesBackend::QueuePicaCompletion(uint64_t id,std::string* error) {
    return Attempt(error,[&] {
        Check(id!=0,"Invalid PICA completion identity");
        Check(completions_.size()<128,"Pending PICA completion limit reached");
        // Like upstream GL, completion acknowledges synchronous copying of guest
        // spans, not a GPU-idle fence. GLES preserves draw order in this context.
        completions_.push_back(id);
    });
}
std::vector<uint64_t> PicaGlesBackend::TakePicaCompletions() {
    auto result=std::move(completions_); completions_.clear(); return result;
}
bool PicaGlesBackend::ResetPicaState(std::string* error) {
    return Attempt(error,[&] {
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before PICA reset");
        glFinish();
        Check(glGetError()==GL_NO_ERROR,"GLES error waiting for PICA reset");
        targets_.clear(); displays_.clear(); pendingFills_.clear(); completions_.clear();
        presentation_.reset(); draws_.Release(); scanout_.Release();
    });
}
void PicaGlesBackend::AbandonContext() {
    for (auto& target : targets_) target.Image->AbandonContext();
    for (auto& display : displays_) display.Image->AbandonContext();
    targets_.clear(); displays_.clear(); pendingFills_.clear(); completions_.clear();
    presentation_.reset(); draws_.AbandonContext(); scanout_.AbandonContext();
}
}

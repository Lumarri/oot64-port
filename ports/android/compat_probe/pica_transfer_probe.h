// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_backend.h"

static void TestPicaTransfers(Fast::Renderer3ds::PicaDrawView draw) {
    using namespace Fast::Renderer3ds;
    Mm3d::Android::PicaGlesBackend backend;
    PicaRenderBackend& runtime=backend;
    std::string error;
    draw.RenderTargetNamespace=9;
    draw.FramebufferColorPhysicalAddress=0x700000; draw.FramebufferDepthPhysicalAddress=0x800000;
    draw.FramebufferWidth=32; draw.FramebufferHeight=64;
    draw.ViewportWidth=32; draw.ViewportHeight=64;
    draw.FragmentShaderSource=R"glsl(#version 450
layout(location=0) out vec4 pica_color;
void main() {
    if (gl_FragCoord.y<32.0) pica_color=gl_FragCoord.x<16.0?vec4(1,0,0,1):vec4(0,1,0,1);
    else pica_color=gl_FragCoord.x<16.0?vec4(0,0,1,1):vec4(1,1,0,1);
}
)glsl";
    const auto submit=[&] {
        if (!runtime.SubmitPicaDraw(draw,&error)) throw std::runtime_error("Transfer draw: "+error);
    };
    const auto transferOk=[&](const PicaDisplayTransferView& transfer) {
        if (!runtime.SubmitPicaDisplayTransfer(transfer,&error)) throw std::runtime_error("Transfer: "+error);
    };
    const auto rejected=[&](bool result) { Require(!result&&!error.empty(),"Unsupported display operation was accepted"); };
    const auto display=[&](uint64_t ns,uint32_t address) -> const Mm3d::Android::GlesRenderTarget& {
        const auto* image=backend.FindDisplay(ns,address);
        Require(image!=nullptr,"Display identity is missing");
        glBindFramebuffer(GL_READ_FRAMEBUFFER,image->Framebuffer());
        return *image;
    };
    submit();
    PicaDisplayTransferView transfer;
    transfer.RenderTargetNamespace=9; transfer.InputPhysicalAddress=0x700000;
    transfer.OutputPhysicalAddress=0x900000;
    transfer.InputWidth=transfer.OutputWidth=32; transfer.InputHeight=transfer.OutputHeight=64;
    transfer.Present=true;
    rejected(runtime.SubmitPicaDisplayTransfer(transfer,&error));
    Require(backend.DisplayCount()==0,"Transfer without configured surface published an image");
    transfer.Present=false; transferOk(transfer);
    display(9,0x900000); TargetPixel(8,16,{255,0,0,255}); TargetPixel(24,48,{255,255,0,255});
    Require(runtime.ClearPicaRenderTarget(9,0x700000,&error),"Source clear failed");
    display(9,0x900000); TargetPixel(8,16,{255,0,0,255}); // Snapshot survives source modification.
    submit();
    Mm3d::Android::GlesRenderTarget window;
    Require(window.Resize(64,64,&error),"Presentation target allocation failed");
    Require(backend.ConfigurePresentation(window.Framebuffer(),64,64,&error),"Presentation configuration failed");
    const auto windowRead=[&] { glBindFramebuffer(GL_READ_FRAMEBUFFER,window.Framebuffer()); };
    transfer.Present=true;
    // Contaminated caller state must not crop, mask, blend or suppress scanout.
    glEnable(GL_SCISSOR_TEST); glScissor(0,0,1,1); glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    glEnable(GL_RASTERIZER_DISCARD); glEnable(GL_BLEND);
    transferOk(transfer); windowRead();
    TargetPixel(16,8,{0,0,0,255}); TargetPixel(16,56,{0,0,0,255});
    TargetPixel(16,24,{0,0,255,255}); TargetPixel(48,24,{255,0,0,255});
    TargetPixel(16,40,{255,255,0,255}); TargetPixel(48,40,{0,255,0,255});
    transfer.Flags=1; transferOk(transfer); windowRead();
    TargetPixel(16,24,{255,0,0,255}); TargetPixel(48,24,{0,0,255,255});
    TargetPixel(16,40,{0,255,0,255}); TargetPixel(48,40,{255,255,0,255});
    transfer.Flags=2|32; transferOk(transfer); windowRead(); // Linear input, no swizzle: same logical orientation as flag 1.
    TargetPixel(16,24,{255,0,0,255}); TargetPixel(48,40,{255,255,0,255});
    // Default framebuffer is also supported (EGL pbuffer here, future window in host).
    Require(backend.ConfigurePresentation(0,64,64,&error),"Default framebuffer configuration failed");
    transferOk(transfer); glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    TargetPixel(16,24,{255,0,0,255}); TargetPixel(16,8,{0,0,0,255});
    Require(backend.ConfigurePresentation(window.Framebuffer(),64,64,&error),"Presentation reconfiguration failed");
    // Invalid requests preserve the published snapshot and never create an image.
    const auto* saved=backend.FindDisplay(9,0x900000);
    const auto invalid=[&](PicaDisplayTransferView bad) {
        rejected(runtime.SubmitPicaDisplayTransfer(bad,&error));
        Require(backend.FindDisplay(9,0x900000)==saved && backend.DisplayCount()==1,"Rejected transfer changed display state");
        display(9,0x900000); TargetPixel(8,16,{255,0,0,255});
    };
    auto bad=transfer; bad.MaterializeGuest=true; invalid(bad);
    bad=transfer; bad.RenderTargetCapture=true; invalid(bad);
    bad=transfer; bad.TextureCopyBytes=128; invalid(bad);
    for (uint32_t flags : {8u,0x10000u,0x100u,0x1000u,0x80000000u,0x3000000u,2u,32u}) {
        bad=transfer; bad.Flags=flags; invalid(bad);
    }
    bad=transfer; bad.InputWidth=31; invalid(bad);
    bad=transfer; bad.OutputHeight=0; invalid(bad);
    bad=transfer; bad.OutputPhysicalAddress=0x700000; invalid(bad);
    bad=transfer; bad.OutputPhysicalAddress=0x900004; invalid(bad);
    bad=transfer; bad.RenderTargetNamespace=10; invalid(bad);
    const auto* source=backend.FindTarget(9,0x700000);
    rejected(backend.ConfigurePresentation(source->Framebuffer(),32,64,&error));
    rejected(backend.ConfigurePresentation(saved->Framebuffer(),32,64,&error));
    rejected(backend.ConfigurePresentation(window.Framebuffer(),0,64,&error));
    PicaMemoryFillView fill{9,0x900000,0x902000,0,0x201};
    rejected(runtime.SubmitPicaMemoryFill(fill,&error));
    auto aliasDraw=draw; aliasDraw.FramebufferColorPhysicalAddress=0x900000;
    rejected(runtime.SubmitPicaDraw(aliasDraw,&error));
    // Independent namespaces may use the same guest output address.
    draw.RenderTargetNamespace=10; submit();
    auto isolated=transfer; isolated.RenderTargetNamespace=10; isolated.Present=false; transferOk(isolated);
    Require(backend.DisplayCount()==2,"Display namespaces were merged");
    display(10,0x900000); TargetPixel(8,16,{255,0,0,255});
    draw.RenderTargetNamespace=9;
    // Downsampling must average real texels, not simply select one corner.
    draw.FragmentShaderSource=R"glsl(#version 450
layout(location=0) out vec4 pica_color;
void main() { pica_color=vec4(float((int(gl_FragCoord.x)&1)==0),float((int(gl_FragCoord.y)&1)==0),64.0/255.0,1); }
)glsl";
    submit();
    transfer.Present=false; transfer.Flags=1u<<24; transfer.OutputWidth=16;
    transferOk(transfer);
    const auto& scaled=display(9,0x900000);
    Require(scaled.Width()==16&&scaled.Height()==64,"Horizontal transfer scale mismatch");
    const auto nearPixel=[&](int x,int y,std::array<unsigned char,4> expected) {
        std::array<unsigned char,4> actual{};
        glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
        Require(glGetError()==GL_NO_ERROR,"Filtered transfer read failed");
        for (unsigned i=0;i<4;++i) Require(std::abs(int(actual[i])-int(expected[i]))<=1,"Filtered transfer color mismatch");
    };
    nearPixel(3,4,{128,255,64,255}); nearPixel(3,5,{128,0,64,255});
    transfer.Flags=2u<<24; transfer.OutputHeight=32; transferOk(transfer);
    const auto& half=display(9,0x900000);
    Require(half.Width()==16&&half.Height()==32,"2x2 transfer scale mismatch");
    nearPixel(3,4,{128,128,64,255});
    // Alpha overlays retain the canvas and black-border area instead of clearing it.
    draw.FragmentShaderSource=R"glsl(#version 450
layout(location=0) out vec4 pica_color;
void main() { pica_color=vec4(0,0,1,0.5); }
)glsl";
    submit();
    glBindFramebuffer(GL_FRAMEBUFFER,window.Framebuffer()); glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    transfer.Present=true; transfer.Flags=0; transfer.OutputWidth=32; transfer.OutputHeight=64;
    transfer.PresentationMode=PicaPresentationMode::AlphaOverlay;
    transferOk(transfer); windowRead(); nearPixel(32,32,{127,0,128,255}); TargetPixel(32,8,{255,0,0,255});
    backend.DisablePresentation();
    saved=backend.FindDisplay(9,0x900000);
    rejected(runtime.SubmitPicaDisplayTransfer(transfer,&error));
    Require(backend.FindDisplay(9,0x900000)==saved,"Disabled presentation replaced snapshot");
    Mm3d::Android::GlesRenderTarget temporaryWindow;
    Require(temporaryWindow.Resize(64,64,&error),"Temporary window allocation failed");
    Require(backend.ConfigurePresentation(temporaryWindow.Framebuffer(),64,64,&error),"Temporary configuration failed");
    temporaryWindow.Release();
    rejected(runtime.SubmitPicaDisplayTransfer(transfer,&error));
    Require(backend.FindDisplay(9,0x900000)==saved,"Failed presentation discarded its display snapshot");
    Require(runtime.ResetPicaState(&error),"Transfer backend reset failed");
    Require(backend.DisplayCount()==0&&backend.TargetCount()==0,"Reset retained transfer state");
    submit(); rejected(runtime.SubmitPicaDisplayTransfer(transfer,&error)); // Reset drops the borrowed surface.
    Require(backend.ConfigurePresentation(window.Framebuffer(),64,64,&error),"Configure after reset failed");
    transfer.PresentationMode=PicaPresentationMode::Replace; transferOk(transfer);
    windowRead(); nearPixel(32,32,{0,0,255,128}); TargetPixel(32,8,{0,0,0,255});
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

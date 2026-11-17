// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_backend.h"

// Borrow the original synthetic shader/geometry fixture while its spans live.
// Calls use the same virtual interface used by the native runtime bridge.
static void TestPicaBackend(Fast::Renderer3ds::PicaDrawView draw) {
    using namespace Fast::Renderer3ds;
    Mm3d::Android::PicaGlesBackend backend;
    PicaRenderBackend& runtime=backend;
    std::string error;
    draw.RenderTargetNamespace=7;
    draw.FramebufferColorPhysicalAddress=0x100000;
    draw.FramebufferDepthPhysicalAddress=0x200000;
    const auto submit=[&] {
        if (!runtime.SubmitPicaDraw(draw,&error)) throw std::runtime_error("PICA backend: "+error);
    };
    const auto pixel=[&](uint64_t ns,uint32_t address,std::array<unsigned char,4> color) {
        const auto* image=backend.FindTarget(ns,address);
        Require(image!=nullptr,"Backend lost target identity");
        glBindFramebuffer(GL_READ_FRAMEBUFFER,image->Framebuffer());
        TargetPixel(32,32,color);
    };
    const auto reject=[&](bool result) { Require(!result&&!error.empty(),"Backend accepted unsupported state"); };
    submit(); pixel(7,0x100000,{255,0,0,255});
    const auto* original=backend.FindTarget(7,0x100000);
    draw.ColorWriteMask=0;
    submit(); Require(original==backend.FindTarget(7,0x100000),"Backend replaced matching target");
    pixel(7,0x100000,{255,0,0,255});
    draw.RenderTargetNamespace=8;
    submit(); pixel(8,0x100000,{0,0,0,0});
    pixel(7,0x100000,{255,0,0,255});
    PicaMemoryFillView fill{8,0x100000,0x104000,0x00FFFFFF,0x201}; // Cyan in native A,B,G,R order.
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Backend color fill failed");
    pixel(8,0x100000,{0,255,255,255});
    // Clear must ignore scissor/write masks left by previous GL users.
    glEnable(GL_SCISSOR_TEST); glScissor(0,0,1,1);
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE); glDepthMask(GL_FALSE); glStencilMask(0);
    Require(runtime.ClearPicaRenderTarget(7,0x100000,&error),"Backend target clear failed");
    pixel(7,0x100000,{0,0,0,0}); pixel(8,0x100000,{0,255,255,255});
    draw.RenderTargetNamespace=7;
    fill={7,0x100000,0x104000,0x00FF00FF,0x201};
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Backend green fill failed");
    pixel(7,0x100000,{0,255,0,255});
    fill={7,0x200000,0x204000,0,0x201};
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Backend depth fill failed");
    draw.ColorWriteMask=15; draw.DepthTestEnabled=true; draw.DepthCompare=PicaCompareFunction::Less;
    submit(); pixel(7,0x100000,{0,255,0,255}); // Depth .5 rejected against filled zero.
    fill.Value=0x07FFFFFF;
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Backend depth/stencil fill failed");
    draw.Stencil.Enabled=true; draw.Stencil.Compare=PicaCompareFunction::Equal;
    draw.Stencil.CompareMask=255; draw.Stencil.Reference=7;
    submit(); pixel(7,0x100000,{255,0,0,255});
    Require(runtime.ClearPicaRenderTarget(7,0x100000,&error),"Backend depth/stencil clear failed");
    draw.Stencil.Reference=0;
    submit(); pixel(7,0x100000,{255,0,0,255}); // Clear restored depth 1 and stencil 0.
    draw.DepthTestEnabled=false; draw.Stencil.Enabled=false;
    // Reject every partial overlap before changing any target.
    fill={7,0x100004,0x104000,0,0x201};
    reject(runtime.SubmitPicaMemoryFill(fill,&error)); pixel(7,0x100000,{255,0,0,255});
    fill.StartPhysicalAddress=0xFFFFC;
    reject(runtime.SubmitPicaMemoryFill(fill,&error)); // Unrepresented bytes before attachment.
    fill={7,0x100000,0x104000,0,0x101}; reject(runtime.SubmitPicaMemoryFill(fill,&error));
    draw.FramebufferWidth=32; reject(runtime.SubmitPicaDraw(draw,&error)); draw.FramebufferWidth=64;
    draw.FramebufferDepthPhysicalAddress=0x200004; reject(runtime.SubmitPicaDraw(draw,&error));
    draw.FramebufferDepthPhysicalAddress=0x200001; reject(runtime.SubmitPicaDraw(draw,&error));
    draw.FramebufferDepthPhysicalAddress=0x200000;
    std::array<uint8_t,256> textureBytes{};
    PicaTextureView texture;
    texture.Width=texture.Height=8; texture.NativeBytes=textureBytes; texture.PhysicalAddress=0x100000;
    draw.Textures={&texture,1}; reject(runtime.SubmitPicaDraw(draw,&error)); draw.Textures={};
    Require(backend.TargetCount()==2,"Rejected alias created a target");
    pixel(7,0x100000,{255,0,0,255});
    // Fill before the first draw is retained once, with latest identical range winning.
    fill={7,0x300000,0x304000,0x00FF00FF,0x201};
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Pending fill failed");
    fill.Value=0x0000FFFF;
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Pending fill replacement failed");
    draw.FramebufferColorPhysicalAddress=0x300000; draw.FramebufferDepthPhysicalAddress=0x400000;
    draw.ColorWriteMask=0;
    const auto savedUniforms=draw.FragmentUniformBytes;
    draw.FragmentUniformBytes={}; reject(runtime.SubmitPicaDraw(draw,&error));
    Require(backend.TargetCount()==2,"Failed draw retained a new target");
    draw.FragmentUniformBytes=savedUniforms;
    submit(); pixel(7,0x300000,{0,0,255,255});
    Require(runtime.ClearPicaRenderTarget(7,0x300000,&error),"Pending target clear failed");
    submit(); pixel(7,0x300000,{0,0,0,0}); // Consumed fill must not replay.
    PicaDisplayTransferView transfer;
    reject(runtime.SubmitPicaDisplayTransfer(transfer,&error));
    reject(runtime.QueuePicaCompletion(0,&error));
    Require(runtime.QueuePicaCompletion(11,&error)&&runtime.QueuePicaCompletion(12,&error),"Completion queue failed");
    Require(runtime.TakePicaCompletions()==std::vector<uint64_t>({11,12}) && runtime.TakePicaCompletions().empty(),
            "Completion order/drain mismatch");
    Require(runtime.QueuePicaCompletion(13,&error),"Completion before reset failed");
    fill={7,0x500000,0x504000,0xFF0000FF,0x201};
    Require(runtime.SubmitPicaMemoryFill(fill,&error),"Pending fill before reset failed");
    Require(runtime.ResetPicaState(&error),"Backend reset failed");
    Require(backend.TargetCount()==0 && runtime.TakePicaCompletions().empty(),"Reset retained backend state");
    draw.FramebufferColorPhysicalAddress=0x500000; draw.FramebufferDepthPhysicalAddress=0x600000;
    submit(); pixel(7,0x500000,{0,0,0,0}); // Reset also drops pending fills.
    draw.ColorWriteMask=15;
    submit(); pixel(7,0x500000,{255,0,0,255});
    glBindFramebuffer(GL_FRAMEBUFFER,0);
}

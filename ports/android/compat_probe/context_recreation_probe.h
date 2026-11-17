// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_backend.h"

// Destroy the old GL context, create an unrelated one with reused object names,
// then discard the old CPU-side backend. It must not delete the new GL objects.
static void TestContextRecreation(Fast::Renderer3ds::PicaDrawView draw) {
    using Mm3d::Android::PicaGlesBackend;
    struct Contexts {
        EGLDisplay Display=eglGetCurrentDisplay();
        EGLContext Original=eglGetCurrentContext(), Old=EGL_NO_CONTEXT, New=EGL_NO_CONTEXT;
        EGLSurface Draw=eglGetCurrentSurface(EGL_DRAW), Read=eglGetCurrentSurface(EGL_READ);
        std::unique_ptr<PicaGlesBackend> OldBackend, NewBackend;
        ~Contexts() {
            // Restore the original context even when an assertion throws.
            if (OldBackend) {
                if (eglGetCurrentContext()!=Old) {
                    if (Old!=EGL_NO_CONTEXT) eglDestroyContext(Display,Old);
                    Old=EGL_NO_CONTEXT; OldBackend->AbandonContext();
                }
                OldBackend.reset();
            }
            if (NewBackend) {
                if (eglGetCurrentContext()!=New) {
                    if (New!=EGL_NO_CONTEXT) eglDestroyContext(Display,New);
                    New=EGL_NO_CONTEXT; NewBackend->AbandonContext();
                }
                NewBackend.reset();
            }
            eglMakeCurrent(Display,Draw,Read,Original);
            if (Old!=EGL_NO_CONTEXT) eglDestroyContext(Display,Old);
            if (New!=EGL_NO_CONTEXT) eglDestroyContext(Display,New);
        }
    } contexts;
    EGLint configId=0,count=0;
    Require(eglQueryContext(contexts.Display,contexts.Original,EGL_CONFIG_ID,&configId),"Context config query failed");
    const EGLint configAttributes[]={EGL_CONFIG_ID,configId,EGL_NONE};
    const EGLint attributes[]={EGL_CONTEXT_MAJOR_VERSION_KHR,3,EGL_CONTEXT_MINOR_VERSION_KHR,2,EGL_NONE};
    EGLConfig config{};
    Require(eglChooseConfig(contexts.Display,configAttributes,&config,1,&count)&&count==1,"Context config unavailable");
    contexts.Old=eglCreateContext(contexts.Display,config,EGL_NO_CONTEXT,attributes);
    Require(contexts.Old!=EGL_NO_CONTEXT &&
            eglMakeCurrent(contexts.Display,contexts.Draw,contexts.Read,contexts.Old),"Temporary old context creation failed");
    draw.FramebufferColorPhysicalAddress=0x110000; draw.FramebufferDepthPhysicalAddress=0x210000;
    std::string error;
    const auto render=[&](PicaGlesBackend& backend) {
        Require(backend.ConfigurePresentation(0,64,64,&error),"Context presentation setup failed");
        if (!backend.SubmitPicaDraw(draw,&error)) throw std::runtime_error("Context draw: "+error);
        Fast::Renderer3ds::PicaDisplayTransferView transfer;
        transfer.RenderTargetNamespace=draw.RenderTargetNamespace;
        transfer.InputPhysicalAddress=draw.FramebufferColorPhysicalAddress; transfer.OutputPhysicalAddress=0x310000;
        transfer.InputWidth=transfer.OutputWidth=draw.FramebufferWidth;
        transfer.InputHeight=transfer.OutputHeight=draw.FramebufferHeight; transfer.Present=true;
        if (!backend.SubmitPicaDisplayTransfer(transfer,&error)) throw std::runtime_error("Context transfer: "+error);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,0); TargetPixel(32,32,{255,0,0,255});
    };
    contexts.OldBackend=std::make_unique<PicaGlesBackend>(); render(*contexts.OldBackend);
    contexts.New=eglCreateContext(contexts.Display,config,EGL_NO_CONTEXT,attributes);
    Require(contexts.New!=EGL_NO_CONTEXT &&
            eglMakeCurrent(contexts.Display,contexts.Draw,contexts.Read,contexts.New),"Replacement context creation failed");
    Require(eglDestroyContext(contexts.Display,contexts.Old),"Old context destruction failed");
    contexts.Old=EGL_NO_CONTEXT;
    contexts.NewBackend=std::make_unique<PicaGlesBackend>(); render(*contexts.NewBackend);
    contexts.OldBackend->AbandonContext(); contexts.OldBackend.reset();
    Require(glGetError()==GL_NO_ERROR,"Abandoning old backend touched the replacement context");
    render(*contexts.NewBackend); // Programs, buffers and both images must still be alive.
}

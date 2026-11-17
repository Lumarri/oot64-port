// SPDX-License-Identifier: GPL-3.0-or-later
// ROM-free visible host: generated TEV + synthetic texture through our GLES backend.
#include "pica_gles_backend.h"
#include "oot3d_native_pica_fragment_shader_gen.h"
#include "fast/extension/pica_uniform_layout.h"
#include "fast/renderer3ds/pica_tev_program.h"
#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace {
using namespace Fast::Renderer3ds;
constexpr const char* tag="MM3DWindow";
int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void Check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> std::span<const uint8_t> Bytes(const T& values) {
    return {reinterpret_cast<const uint8_t*>(values.data()),sizeof(values)};
}
class WindowSession {
public:
    ~WindowSession() { Stop(); }
    void Start(ANativeWindow* window) {
        Stop();
        display_=eglGetDisplay(EGL_DEFAULT_DISPLAY);
        Check(display_!=EGL_NO_DISPLAY && eglInitialize(display_,nullptr,nullptr),"EGL display initialization failed");
        Check(eglBindAPI(EGL_OPENGL_ES_API),"EGL GLES API unavailable");
        const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,0,EGL_NONE};
        EGLConfig config{}; EGLint count=0,format=0;
        Check(eglChooseConfig(display_,attributes,&config,1,&count)&&count==1,"EGL window config unavailable");
        Check(eglGetConfigAttrib(display_,config,EGL_NATIVE_VISUAL_ID,&format),"EGL native format unavailable");
        Check(ANativeWindow_setBuffersGeometry(window,0,0,format)==0,"Native buffer configuration failed");
        surface_=eglCreateWindowSurface(display_,config,window,nullptr);
        Check(surface_!=EGL_NO_SURFACE,"EGL window surface creation failed");
        const EGLint contextAttributes[]={EGL_CONTEXT_MAJOR_VERSION_KHR,3,EGL_CONTEXT_MINOR_VERSION_KHR,2,EGL_NONE};
        context_=eglCreateContext(display_,config,EGL_NO_CONTEXT,contextAttributes);
        Check(context_!=EGL_NO_CONTEXT && eglMakeCurrent(display_,surface_,surface_,context_),"EGL GLES 3.2 context creation failed");
        backend_=std::make_unique<Mm3d::Android::PicaGlesBackend>();
        std::string error;
        Check(Oot3dNativeGame::GenerateOot3dPicaParametricBaseFragmentShader(
            Oot3dNativeGame::Oot3dPicaParametricLighting::Off,false,fragment_,&error),"TEV generation failed");
        packed_.fill(0);
        PicaTevProgram tev;
        for (auto& stage : tev.Stages) stage[0]=3u|(3u<<16);
        tev.Control[1]=1;
        std::memcpy(packed_.data()+Fast::Extension::kPicaPackedFragmentTevProgramOffset,&tev,sizeof(tev));
        const uint32_t enableTexture=1;
        std::memcpy(packed_.data()+Fast::Extension::kPicaPackedFragmentControlOffset+8,&enableTexture,4);
        for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
            unsigned morton=0;
            for (unsigned bit=0;bit<3;++bit) morton|=((x>>bit)&1)<<(bit*2)|((y>>bit)&1)<<(bit*2+1);
            // Bright teal / gold / blue / purple quadrants; A,B,G,R native order.
            const std::array<std::array<uint8_t,4>,4> colors{{{30,210,175,255},{245,190,55,255},
                                                           {55,115,235,255},{175,75,205,255}}};
            const auto& color=colors[(y/4)*2+x/4];
            for (unsigned channel=0;channel<4;++channel) texture_[morton*4+channel]=color[3-channel];
        }
        createdMs_=NowMs(); frames_=0; width_=height_=0;
        __android_log_print(ANDROID_LOG_INFO,tag,"context_created renderer=%s gles=%s scope=synthetic_not_gameplay",
            glGetString(GL_RENDERER),glGetString(GL_VERSION));
    }
    void Stop() {
        if (backend_) {
            // GL names must never be deleted in a different/new EGL context.
            if (!lost_ && eglGetCurrentContext()==context_) backend_.reset();
            else { backend_->AbandonContext(); backend_.reset(); }
        }
        if (display_!=EGL_NO_DISPLAY) {
            eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if (context_!=EGL_NO_CONTEXT) eglDestroyContext(display_,context_);
            if (surface_!=EGL_NO_SURFACE) eglDestroySurface(display_,surface_);
            eglTerminate(display_);
            __android_log_print(ANDROID_LOG_INFO,tag,"context_released frames=%u",frames_);
        }
        display_=EGL_NO_DISPLAY; context_=EGL_NO_CONTEXT; surface_=EGL_NO_SURFACE;
        lost_=false; width_=height_=0;
    }
    bool Ready() const { return backend_!=nullptr; }
    void Render() {
        EGLint width=0,height=0;
        Check(eglQuerySurface(display_,surface_,EGL_WIDTH,&width)&&eglQuerySurface(display_,surface_,EGL_HEIGHT,&height),
              "Window dimensions unavailable");
        std::string error;
        if (width!=width_ || height!=height_) {
            if (!backend_->ConfigurePresentation(0,width,height,&error)) throw std::runtime_error(error);
            width_=width; height_=height;
            __android_log_print(ANDROID_LOG_INFO,tag,"surface_resized width=%d height=%d",width_,height_);
        }
        constexpr std::array<float,12> positions{-1,-1,-0.5f,1, 3,-1,-0.5f,1, -1,3,-0.5f,1};
        const std::array<float,4> pan{float(std::fmod(double(NowMs()-createdMs_)/12000.,1.)),0,0,0};
        const PicaVertexBindingView binding{0,16,false,Bytes(positions)};
        const PicaVertexAttributeView attribute{0,0,PicaVertexFormat::Float,4,0};
        std::array<PicaTextureView,3> textures{};
        for (unsigned i=0;i<3;++i) {
            textures[i].Slot=i; textures[i].Width=textures[i].Height=8;
            textures[i].PhysicalAddress=0x500000; textures[i].NativeBytes=texture_;
            textures[i].NativeWrapS=textures[i].NativeWrapT=2;
        }
        constexpr const char* vertex=R"glsl(#version 450
layout(location=0) in vec4 pica_input;
layout(set=0,binding=0,std140) uniform VertexUniforms { vec4 pan; } uniforms;
layout(location=0) out vec4 pica_primary_color;
layout(location=1) out vec2 pica_texcoord0;
layout(location=2) out vec2 pica_texcoord1;
layout(location=3) out vec2 pica_texcoord2;
layout(location=4) out float pica_texcoord0_w;
layout(location=5) out vec4 pica_normquat;
layout(location=6) out vec3 pica_view;
void main() {
    pica_primary_color=vec4(1);
    pica_texcoord0=pica_texcoord1=pica_texcoord2=pica_input.xy*0.5+0.5+uniforms.pan.xy;
    pica_texcoord0_w=1.0; pica_normquat=vec4(0,0,0,1); pica_view=vec3(0,0,1);
    vec4 pica_position=pica_input;
    gl_Position = vec4(pica_position.x, pica_position.y, -pica_position.z, pica_position.w);
}
)glsl";
        PicaDrawView draw;
        draw.SubmissionId=frames_+1; draw.RenderTargetNamespace=1;
        draw.FramebufferWidth=240; draw.FramebufferHeight=400;
        draw.FramebufferColorPhysicalAddress=0x100000; draw.FramebufferDepthPhysicalAddress=0x200000;
        draw.FramebufferColorFormat=0; draw.FramebufferDepthFormat=3;
        draw.ViewportWidth=240; draw.ViewportHeight=400; draw.ColorWriteMask=15;
        draw.VertexShaderSource=vertex; draw.FragmentShaderSource=fragment_.Source;
        draw.VertexUniformBytes=Bytes(pan); draw.FragmentUniformBytes=packed_;
        draw.VertexBindings={&binding,1}; draw.VertexAttributes={&attribute,1}; draw.VertexCount=3; draw.Textures=textures;
        if (!backend_->SubmitPicaDraw(draw,&error)) throw std::runtime_error(error);
        PicaDisplayTransferView transfer;
        transfer.RenderTargetNamespace=1; transfer.InputPhysicalAddress=0x100000; transfer.OutputPhysicalAddress=0x300000;
        transfer.InputWidth=transfer.OutputWidth=240; transfer.InputHeight=transfer.OutputHeight=400;
        transfer.AfterDrawSubmissionId=draw.SubmissionId; transfer.Present=true;
        if (!backend_->SubmitPicaDisplayTransfer(transfer,&error)) throw std::runtime_error(error);
        if (frames_==0) {
            std::array<uint8_t,4> pixel{};
            glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER,0); glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadPixels(width_/2,height_/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
            Check(glGetError()==GL_NO_ERROR && pixel[3]==255 && (pixel[0]||pixel[1]||pixel[2]),"Visible framebuffer readback failed");
            __android_log_print(ANDROID_LOG_INFO,tag,"frame_readback rgba=%u,%u,%u,%u",pixel[0],pixel[1],pixel[2],pixel[3]);
        }
        if (!eglSwapBuffers(display_,surface_)) {
            lost_=eglGetError()==EGL_CONTEXT_LOST;
            throw std::runtime_error("EGL window swap failed");
        }
        ++frames_;
        if (frames_==1) __android_log_print(ANDROID_LOG_INFO,tag,"frame_presented scope=synthetic_not_gameplay");
    }
private:
    EGLDisplay display_=EGL_NO_DISPLAY;
    EGLSurface surface_=EGL_NO_SURFACE;
    EGLContext context_=EGL_NO_CONTEXT;
    bool lost_=false;
    EGLint width_=0,height_=0;
    unsigned frames_=0;
    int64_t createdMs_=0;
    std::unique_ptr<Mm3d::Android::PicaGlesBackend> backend_;
    Oot3dNativeGame::Oot3dPicaGeneratedFragmentShader fragment_;
    std::array<uint8_t,Fast::Extension::kPicaPackedFragmentUniformSize> packed_{};
    std::array<uint8_t,256> texture_{};
};
struct AppState { WindowSession Window; bool Resumed=false,Focused=false; };
void Command(android_app* app,int32_t command) {
    auto& state=*static_cast<AppState*>(app->userData);
    try {
        switch (command) {
        case APP_CMD_INIT_WINDOW: if (app->window) state.Window.Start(app->window); break;
        case APP_CMD_TERM_WINDOW: state.Window.Stop(); break;
        case APP_CMD_RESUME: state.Resumed=true; __android_log_print(ANDROID_LOG_INFO,tag,"activity_resumed"); break;
        case APP_CMD_PAUSE: state.Resumed=false; __android_log_print(ANDROID_LOG_INFO,tag,"activity_paused"); break;
        case APP_CMD_GAINED_FOCUS: state.Focused=true; break;
        case APP_CMD_LOST_FOCUS: state.Focused=false; break;
        default: break;
        }
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_ERROR,tag,"fatal_startup %s",error.what());
        state.Window.Stop(); ANativeActivity_finish(app->activity);
    }
}
}
void android_main(android_app* app) {
    ANativeActivity_setWindowFlags(app->activity,AWINDOW_FLAG_KEEP_SCREEN_ON,0);
    AppState state;
    app->userData=&state; app->onAppCmd=Command;
    int64_t nextFrame=0;
    while (!app->destroyRequested) {
        const bool render=state.Resumed&&state.Focused&&state.Window.Ready();
        const int timeout=render?int(std::max<int64_t>(0,nextFrame-NowMs())):-1;
        android_poll_source* source=nullptr; int events=0;
        const int result=ALooper_pollOnce(timeout,nullptr,&events,reinterpret_cast<void**>(&source));
        if (result>=0 && source) source->process(app,source);
        if (app->destroyRequested) break;
        if (state.Resumed&&state.Focused&&state.Window.Ready()&&NowMs()>=nextFrame) {
            try { state.Window.Render(); }
            catch (const std::exception& error) {
                __android_log_print(ANDROID_LOG_ERROR,tag,"fatal_render %s",error.what());
                state.Window.Stop(); ANativeActivity_finish(app->activity);
            }
            nextFrame=NowMs()+33; // Synthetic prototype capped at ~30 updates/s.
        }
    }
    state.Window.Stop();
    app->userData=nullptr;
}

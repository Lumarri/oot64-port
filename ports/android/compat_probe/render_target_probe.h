// SPDX-License-Identifier: GPL-3.0-or-later
#include "gles_render_target.h"

static void TargetPixel(int x, int y, std::array<unsigned char,4> expected) {
    std::array<unsigned char,4> pixel{};
    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
    Require(glGetError() == GL_NO_ERROR, "Render target read failed");
    if (pixel != expected) {
        std::string detail = "Render target pixel mismatch at " + std::to_string(x) + "," + std::to_string(y);
        detail += " actual=";
        for (auto channel : pixel) detail += std::to_string(channel) + ",";
        detail += " expected=";
        for (auto channel : expected) detail += std::to_string(channel) + ",";
        throw std::runtime_error(detail);
    }
}

static void TestRenderTargets() {
    Mm3d::Android::GlesRenderTarget top, bottom, composition;
    std::string error;
    Require(top.Resize(400,240,&error), "Top target allocation failed");
    glBindFramebuffer(GL_FRAMEBUFFER,top.Framebuffer());
    glViewport(0,0,400,240);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(255);
    glClearColor(0,0,1,1);
    glClearDepthf(0.25f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0,0,200,240);
    glClearStencil(1);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    constexpr const char* vertex = R"glsl(#version 320 es
precision highp float;
void main() {
    const vec2 p[3] = vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    gl_Position = vec4(p[gl_VertexID],0,1);
}
)glsl";
    constexpr const char* fragment = R"glsl(#version 320 es
precision highp float;
layout(location=0) out vec4 color;
void main() { color = vec4(0,1,0,1); }
)glsl";
    const GLuint program = Program({Shader(GL_VERTEX_SHADER,vertex),Shader(GL_FRAGMENT_SHADER,fragment)});
    glUseProgram(program);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL,1,255);
    glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    glDrawArrays(GL_TRIANGLES,0,3);
    TargetPixel(100,120,{0,0,255,255}); // Depth 0.5 is rejected by clear depth 0.25.
    glClearDepthf(0.75f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES,0,3);
    TargetPixel(100,120,{0,255,0,255});
    TargetPixel(300,120,{0,0,255,255}); // Stencil rejects right half.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    const GLuint original = top.Framebuffer();
    Require(top.Resize(400,240,&error) && original == top.Framebuffer(), "Same-size target was replaced");
    TargetPixel(100,120,{0,255,0,255});
    GLint limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
    Require(!top.Resize(0,240,&error) && !error.empty(), "Zero target width accepted");
    Require(!top.Resize(limit+1,240,&error) && !error.empty(), "Oversized target accepted");
    Require(top.Framebuffer() == original, "Rejected resize replaced the target");
    TargetPixel(100,120,{0,255,0,255});
    Require(bottom.Resize(320,240,&error), "Bottom target allocation failed");
    GLint binding = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&binding);
    Require(static_cast<GLuint>(binding) == top.Framebuffer(), "Allocation changed unrelated framebuffer binding");
    glBindFramebuffer(GL_FRAMEBUFFER,bottom.Framebuffer());
    glClearColor(1,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    Require(composition.Resize(400,480,&error), "Composition target allocation failed");
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,composition.Framebuffer());
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,top.Framebuffer());
    glBlitFramebuffer(0,0,400,240,0,240,400,480,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,bottom.Framebuffer());
    glBlitFramebuffer(0,0,320,240,40,0,360,240,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,composition.Framebuffer());
    TargetPixel(100,360,{0,255,0,255});
    TargetPixel(300,360,{0,0,255,255});
    TargetPixel(200,120,{255,0,0,255});
    TargetPixel(20,120,{0,0,0,255});
    // Reallocate while bound, preserving the binding's meaning, not its old ID.
    glBindFramebuffer(GL_FRAMEBUFFER,top.Framebuffer());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D,top.Color());
    Require(top.Resize(64,64,&error), "Bound target resize failed");
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&binding);
    Require(static_cast<GLuint>(binding) == top.Framebuffer(), "Draw binding not remapped");
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&binding);
    Require(static_cast<GLuint>(binding) == top.Framebuffer(), "Read binding not remapped");
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
    Require(static_cast<GLuint>(binding) == top.Color(), "Texture binding not remapped");
    Require(glIsFramebuffer(original) == GL_FALSE, "Previous framebuffer leaked after resize");
    glViewport(0,0,64,64);
    // Run the real fragment generator/TEV/logic/depth tests in our owned target.
    TestGeneratedPicaFragments();
    glDeleteProgram(program);
    const GLuint releasedFramebuffer = top.Framebuffer(), releasedTexture = top.Color();
    top.Release();
    Require(!top.Framebuffer() && !top.Color() && !top.Width() && !top.Height(), "Release did not reset ownership");
    Require(!glIsFramebuffer(releasedFramebuffer) && !glIsTexture(releasedTexture), "Target objects survived release");
    Require(top.Resize(64,64,&error), "Target recreation failed");
    glBindFramebuffer(GL_FRAMEBUFFER,top.Framebuffer());
    glClearColor(0,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT);
    TargetPixel(32,32,{0,255,255,255});
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glActiveTexture(GL_TEXTURE0);
    glClearDepthf(1.0f);
    glClearStencil(0);
}

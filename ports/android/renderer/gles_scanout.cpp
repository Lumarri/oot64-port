// SPDX-License-Identifier: GPL-3.0-or-later
#include "gles_scanout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace Mm3d::Android {
namespace {
void Check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
GLuint Compile(GLenum stage,const char* source) {
    const GLuint shader=glCreateShader(stage);
    glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
    GLint okay=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
    if (!okay) {
        std::array<char,2048> log{};
        glGetShaderInfoLog(shader,log.size(),nullptr,log.data()); glDeleteShader(shader);
        throw std::runtime_error(std::string("GLES scanout shader: ")+log.data());
    }
    return shader;
}
GLuint Build() {
    constexpr const char* vertex=R"glsl(#version 320 es
precision highp float;
out vec2 uv;
void main() {
    const vec2 corners[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vec2 p=corners[gl_VertexID];
    gl_Position=vec4(p,0,1); uv=p*0.5+0.5;
}
)glsl";
    constexpr const char* fragment=R"glsl(#version 320 es
precision highp float;
precision highp int;
precision highp sampler2D;
layout(binding=0) uniform sampler2D image;
uniform int flip_y;
in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    vec2 portrait_uv=uv.yx;
    if (flip_y!=0) portrait_uv.y=1.0-portrait_uv.y;
    color=texture(image,portrait_uv);
}
)glsl";
    GLuint vs=0,fs=0,program=0;
    try {
        vs=Compile(GL_VERTEX_SHADER,vertex); fs=Compile(GL_FRAGMENT_SHADER,fragment);
        program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program);
        GLint okay=0; glGetProgramiv(program,GL_LINK_STATUS,&okay);
        Check(okay,"GLES scanout program link failed");
        glDetachShader(program,vs); glDetachShader(program,fs);
        glDeleteShader(vs); glDeleteShader(fs); return program;
    } catch (...) { glDeleteShader(vs); glDeleteShader(fs); glDeleteProgram(program); throw; }
}
}
void GlesScanout::Release() {
    if (program_) glDeleteProgram(program_);
    if (vao_) glDeleteVertexArrays(1,&vao_);
    AbandonContext();
}
void GlesScanout::AbandonContext() {
    program_=vao_=0;
}
bool GlesScanout::Present(const GlesRenderTarget& image,GLuint destination,GLsizei width,GLsizei height,
                         bool flip,bool overlay,std::string* error) {
    if (error) error->clear();
    try {
        Check(image.Framebuffer() && image.Framebuffer()!=destination,"Invalid or self-referencing GLES scanout");
        GLint limits[2]{}; glGetIntegerv(GL_MAX_VIEWPORT_DIMS,limits);
        Check(width>0 && height>0 && width<=limits[0] && height<=limits[1],"Invalid GLES scanout extent");
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before scanout");
        Check(destination==0 || glIsFramebuffer(destination),"GLES scanout destination was deleted");
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,destination);
        Check(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete GLES scanout destination");
        if (!program_) { program_=Build(); glGenVertexArrays(1,&vao_); }
        glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST); glDisable(GL_DITHER); glDisable(GL_RASTERIZER_DISCARD);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_COVERAGE);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        if (overlay) {
            glEnable(GL_BLEND); glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
        }
        // Native memory layout is portrait: the logical display has transposed dimensions.
        const double scale=std::min(double(width)/image.Height(),double(height)/image.Width());
        const auto vw=std::max<GLsizei>(1,std::lround(image.Height()*scale));
        const auto vh=std::max<GLsizei>(1,std::lround(image.Width()*scale));
        glViewport((width-vw)/2,(height-vh)/2,vw,vh);
        glBindVertexArray(vao_); glUseProgram(program_);
        glUniform1i(glGetUniformLocation(program_,"flip_y"),flip);
        glActiveTexture(GL_TEXTURE0); glBindSampler(0,0); glBindTexture(GL_TEXTURE_2D,image.Color());
        glDrawArrays(GL_TRIANGLES,0,3);
        Check(glGetError()==GL_NO_ERROR,"GLES error presenting transfer image");
        return true;
    } catch (const std::exception& failure) { if (error) *error=failure.what(); return false; }
}
}

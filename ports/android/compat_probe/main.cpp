// Synthetic GPU capability test. No game code or assets.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

static bool HasExtension(const std::string& requested) {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
        const auto* name = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        if (name && requested == name) return true;
    }
    return false;
}

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static GLuint Shader(GLenum stage, const char* source) {
    GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        std::array<char, 4096> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Shader compilation: ") + log.data());
    }
    return shader;
}

static GLuint Program(std::initializer_list<GLuint> shaders) {
    GLuint program = glCreateProgram();
    for (auto shader : shaders) glAttachShader(program, shader);
    glLinkProgram(program);
    for (auto shader : shaders) glDeleteShader(shader);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::array<char, 4096> log{};
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error(std::string("Program link: ") + log.data());
    }
    return program;
}

#include "pica_dialect_probe.h"

int main() {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int status = 1;
    try {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        Require(display != EGL_NO_DISPLAY, "eglGetDisplay");
        Require(eglInitialize(display, nullptr, nullptr), "eglInitialize");
        Require(eglBindAPI(EGL_OPENGL_ES_API), "eglBindAPI");
        const EGLint configAttributes[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_NONE};
        EGLConfig config{};
        EGLint count = 0;
        Require(eglChooseConfig(display, configAttributes, &config, 1, &count) && count == 1,
                "No RGBA8/depth24 GLES3 pbuffer configuration");
        const EGLint surfaceAttributes[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
        Require(surface != EGL_NO_SURFACE, "eglCreatePbufferSurface");
        const EGLint contextAttributes[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 2, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        Require(context != EGL_NO_CONTEXT, "GLES 3.2 context unavailable");
        Require(eglMakeCurrent(display, surface, surface, context), "eglMakeCurrent");
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        Require(major > 3 || (major == 3 && minor >= 2), "Context is below GLES 3.2");

        constexpr const char* vertex = R"glsl(#version 320 es
precision highp float;
void main() {
    const vec2 vertices[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
})glsl";
        constexpr const char* fragment = R"glsl(#version 320 es
precision highp float;
layout(location=0) out vec4 color;
void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }
)glsl";
        const GLuint graphics = Program({Shader(GL_VERTEX_SHADER, vertex), Shader(GL_FRAGMENT_SHADER, fragment)});
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glViewport(0, 0, 64, 64);
        glDisable(GL_DITHER);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(graphics);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::array<unsigned char, 4> pixel{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        Require(glGetError() == GL_NO_ERROR, "GLES draw/readback error");
        Require(pixel[0] <= 1 && pixel[1] >= 254 && pixel[2] <= 1 && pixel[3] >= 254,
                "Triangle readback did not match green RGBA pixel");

        constexpr const char* compute = R"glsl(#version 320 es
precision highp int;
layout(local_size_x=1) in;
layout(std430, binding=0) buffer Result { uint value; };
void main() { value = 0x4d4d3344u; }
)glsl";
        const GLuint computeProgram = Program({Shader(GL_COMPUTE_SHADER, compute)});
        GLuint buffer = 0;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        std::uint32_t value = 0;
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(value), &value, GL_DYNAMIC_READ);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
        glUseProgram(computeProgram);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
        const auto* mapped = static_cast<const std::uint32_t*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(value), GL_MAP_READ_BIT));
        Require(mapped != nullptr, "SSBO map failed");
        value = *mapped;
        Require(glUnmapBuffer(GL_SHADER_STORAGE_BUFFER), "SSBO unmap failed");
        Require(glGetError() == GL_NO_ERROR && value == 0x4d4d3344u, "Compute/SSBO readback mismatch");

        TestPicaDialect();
        std::cout << "{\"renderer\":" << std::quoted(reinterpret_cast<const char*>(glGetString(GL_RENDERER)))
                  << ",\"gles\":" << std::quoted(reinterpret_cast<const char*>(glGetString(GL_VERSION)))
                  << ",\"ext_shader_framebuffer_fetch\":" << (HasExtension("GL_EXT_shader_framebuffer_fetch") ? "true" : "false")
                  << ",\"arm_shader_framebuffer_fetch\":" << (HasExtension("GL_ARM_shader_framebuffer_fetch") ? "true" : "false")
                  << ",\"ext_texture_border_clamp\":" << (HasExtension("GL_EXT_texture_border_clamp") ? "true" : "false")
                  << ",\"rgba8_depth24_draw_readback\":true,\"compute_ssbo_readback\":true,"
                     "\"pica_dialect_texture_ubo_depth\":true,\"dialect_rejections\":true,"
                     "\"scope\":\"synthetic_offscreen_not_full_pica_or_gameplay\"}\n";
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(graphics);
        glDeleteProgram(computeProgram);
        status = 0;
    } catch (const std::exception& error) {
        std::cerr << "GLES compatibility probe: " << error.what() << '\n';
    }
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        eglTerminate(display);
    }
    return status;
}

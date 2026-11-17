// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_logic.h"
#include <vector>

static void TestPicaLogic() {
    Require(HasExtension("GL_EXT_shader_framebuffer_fetch"),
            "Coherent EXT framebuffer fetch required for RGBA8 logic probe");
    constexpr const char* vertex = R"glsl(#version 320 es
precision highp float;
void main() {
    const vec2 p[3] = vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    gl_Position = vec4(p[gl_VertexID],0,1);
}
)glsl";
    const std::string fragment = std::string(R"glsl(#version 320 es
#extension GL_EXT_shader_framebuffer_fetch : require
precision highp float;
precision highp int;
layout(location=0) inout vec4 color;
uniform int operation;
)glsl") + std::string(Mm3d::Android::PicaRgba8LogicGlsl) + R"glsl(
void main() {
    uint x = uint(gl_FragCoord.x);
    vec4 source = vec4(uvec4(x, 255u-x, x^85u, x^170u)) / 255.0;
    color = pica_logic_rgba8(source, color, operation);
}
)glsl";
    const GLuint program = Program({Shader(GL_VERTEX_SHADER, vertex),
                                    Shader(GL_FRAGMENT_SHADER, fragment.c_str())});
    const GLint operationUniform = glGetUniformLocation(program, "operation");
    Require(operationUniform >= 0, "Logic operation uniform missing");
    GLuint target = 0, framebuffer = 0;
    glGenTextures(1, &target);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    std::vector<unsigned char> initial(256 * 256 * 4), actual(initial.size());
    for (unsigned y = 0; y < 256; ++y) {
        for (unsigned x = 0; x < 256; ++x) {
            const size_t offset = (y * 256 + x) * 4;
            initial[offset] = static_cast<unsigned char>(y);
            initial[offset+1] = static_cast<unsigned char>(255-y);
            initial[offset+2] = static_cast<unsigned char>(y^51);
            initial[offset+3] = static_cast<unsigned char>(y^204);
        }
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,initial.data());
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &attachment);
    Require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
            "Logic RGBA8 framebuffer incomplete");
    glViewport(0,0,256,256);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(program);
    // Independent one-bit truth tables, indexed by (source_bit*2+dest_bit).
    constexpr unsigned tables[16] = {0,8,4,12,15,3,10,5,7,14,1,6,9,2,13,11};
    for (int op = 0; op < 16; ++op) {
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,initial.data());
        std::vector<unsigned char> expected = initial;
        glUniform1i(operationUniform, op);
        for (int pass = 0; pass < 2; ++pass) {
            // Second draw checks coherent destination updates and color masks.
            glColorMask(GL_TRUE,pass == 0,GL_TRUE,pass == 0);
            glDrawArrays(GL_TRIANGLES,0,3);
            for (unsigned y = 0; y < 256; ++y) {
                for (unsigned x = 0; x < 256; ++x) {
                    const unsigned source[4] = {x,255-x,x^85,x^170};
                    for (unsigned c = 0; c < 4; ++c) {
                        if (pass && (c == 1 || c == 3)) continue;
                        const size_t index = (y*256+x)*4+c;
                        unsigned value = 0;
                        for (unsigned bit = 0; bit < 8; ++bit) {
                            unsigned entry = (((source[c]>>bit)&1)*2) + ((expected[index]>>bit)&1);
                            value |= ((tables[op]>>entry)&1)<<bit;
                        }
                        expected[index] = static_cast<unsigned char>(value);
                    }
                }
            }
            glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
            Require(glGetError() == GL_NO_ERROR, "Logic framebuffer fetch GL error");
            if (actual != expected)
                throw std::runtime_error("RGBA8 logic mismatch: operation=" + std::to_string(op)
                                         + " pass=" + std::to_string(pass));
        }
    }
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,64,64);
    glDeleteFramebuffers(1,&framebuffer);
    glDeleteTextures(1,&target);
    glDeleteProgram(program);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Original synthetic fixtures, not ROM-derived shaders. Included after the
// probe's Require/Shader/Program helpers.
#include "pica_gles_shader_dialect.h"

static void TestPicaDialect() {
    constexpr const char* vertex = R"glsl(#version 450
layout(set=0,binding=0,std140) uniform PicaVertexUniforms { vec4 f[96]; } uniforms;
void main() {
    const vec2 vertices[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
    vec4 pica_position = vec4(vertices[gl_VertexID] * 2.0, -uniforms.f[0].x * 2.0, 2.0);
    gl_Position = vec4(pica_position.x, pica_position.y, -pica_position.z, pica_position.w);
}
)glsl";
    constexpr const char* fragment = R"glsl(#version 450
layout(set = 0, binding = 2) uniform sampler2D pica_texture1;
layout(set=0,binding=13) uniform usampler2D pica_lighting_lut;
layout(location=0) out vec4 pica_color;
void main() {
    pica_color = vec4(float(texelFetch(pica_lighting_lut, ivec2(0), 0).r) / 255.0,
                      texture(pica_texture1, vec2(0.5)).g, gl_FragCoord.z, 1.0);
}
)glsl";
    std::string translatedVertex, translatedFragment, error;
    Require(Mm3d::Android::TranslatePicaShaderToGles32(vertex, translatedVertex, &error),
            "Canonical vertex translation failed");
    Require(Mm3d::Android::TranslatePicaShaderToGles32(fragment, translatedFragment, &error),
            "Canonical fragment translation failed");
    for (const char* rejected : {
        "#version 430\nvoid main() {}",
        "#version 450\nlayout(set=1,binding=0) uniform sampler2D t;",
        "#version 450\nlayout(push_constant) uniform State { int x; } s;",
        "#version 450\nuniform texture2D t;",
        "#version 450\n#extension GL_ARB_clip_control : require\n"}) {
        std::string output = "stale shader";
        Require(!Mm3d::Android::TranslatePicaShaderToGles32(rejected, output, &error)
                && output.empty() && !error.empty(), "Unsupported dialect was not rejected cleanly");
    }
    const GLuint program = Program({Shader(GL_VERTEX_SHADER, translatedVertex.c_str()),
                                    Shader(GL_FRAGMENT_SHADER, translatedFragment.c_str())});
    GLuint ubo = 0;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    std::array<float, 96 * 4> uniforms{};
    uniforms[0] = 0.25f;
    glBufferData(GL_UNIFORM_BUFFER, sizeof(uniforms), uniforms.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    GLuint textures[2]{};
    glGenTextures(2, textures);
    const std::array<unsigned char, 4> texel{0, 128, 0, 255};
    const GLuint lut = 64;
    for (int i = 0; i < 2; ++i) {
        glActiveTexture(i == 0 ? GL_TEXTURE2 : GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, i == 0 ? GL_RGBA8 : GL_R32UI, 1, 1, 0,
                     i == 0 ? GL_RGBA : GL_RED_INTEGER,
                     i == 0 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_INT,
                     i == 0 ? static_cast<const void*>(texel.data()) : &lut);
    }
    glUseProgram(program);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glClearDepthf(1.0f);
    glClearColor(0, 0, 0, 1);
    const auto draw = [&](float depth) {
        uniforms[0] = depth;
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uniforms), uniforms.data());
        glDrawArrays(GL_TRIANGLES, 0, 3);
    };
    const auto checkPixel = [](int red, int green, int blue) {
        std::array<unsigned char, 4> pixel{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        Require(glGetError() == GL_NO_ERROR, "PICA dialect GL error");
        Require(int(pixel[0]) >= red - 1 && int(pixel[0]) <= red + 1
                && int(pixel[1]) >= green - 1 && int(pixel[1]) <= green + 1
                && int(pixel[2]) >= blue - 1 && int(pixel[2]) <= blue + 1 && pixel[3] == 255,
                "PICA dialect texture/UBO/clip-depth/occlusion mismatch");
    };
    // Independent depths catch a broken UBO update; w=2 catches incorrect
    // homogeneous depth conversion. Both draw orders must preserve the front.
    for (float depth : {0.25f, 0.75f}) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw(depth);
        checkPixel(64, 128, depth == 0.25f ? 64 : 191);
        draw(1.0f - depth);
        checkPixel(64, 128, 64);
    }
    for (float depth : {-0.25f, 1.25f}) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        draw(depth);
        checkPixel(0, 0, 0);
    }
    glDeleteTextures(2, textures);
    glDeleteBuffers(1, &ubo);
    glDeleteProgram(program);
}

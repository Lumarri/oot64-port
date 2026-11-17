// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the pinned upstream generator without game code or ROM data.
#include "oot3d_native_pica_fragment_shader_gen.h"

static void DrawGeneratedPica(GLuint program, bool logic = false) {
    const GLuint block = glGetUniformBlockIndex(program, "PicaFragmentUniforms");
    Require(block != GL_INVALID_INDEX, "Generated PICA uniform block missing");
    GLint bytes = 0;
    glGetActiveUniformBlockiv(program, block, GL_UNIFORM_BLOCK_DATA_SIZE, &bytes);
    Require(bytes > 0, "Generated PICA uniform block is empty");
    // Zero TEV words select primary color with replace operations in all six
    // stages. Fog, alpha test and lighting are disabled in this controlled draw.
    std::vector<unsigned char> uniforms(static_cast<size_t>(bytes), 0);
    GLuint ubo = 0, texture = 0;
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, bytes, uniforms.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 4, ubo);
    glGenTextures(1, &texture);
    const std::array<unsigned char, 4> white{255,255,255,255};
    for (GLenum unit : {GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3}) {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
    glUseProgram(program);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    constexpr std::array<unsigned char,4> source{64,128,191,255}, destination{85,170,51,204};
    constexpr unsigned tables[16] = {0,8,4,12,15,3,10,5,7,14,1,6,9,2,13,11};
    const GLint operation = glGetUniformLocation(program, "mm3d_logic_operation");
    Require(!logic || operation >= 0, "Generated PICA logic uniform missing");
    for (int op = 0; op < (logic ? 16 : 1); ++op) {
        glClearColor(destination[0]/255.f,destination[1]/255.f,destination[2]/255.f,destination[3]/255.f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (logic) glUniform1i(operation, op);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::array<unsigned char, 4> pixel{}, expected = source;
        if (logic) {
            for (unsigned c = 0; c < 4; ++c) {
                unsigned value = 0;
                for (unsigned bit = 0; bit < 8; ++bit) {
                    unsigned entry = (((source[c]>>bit)&1)*2) + ((destination[c]>>bit)&1);
                    value |= ((tables[op]>>entry)&1)<<bit;
                }
                expected[c] = static_cast<unsigned char>(value);
            }
        }
        glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        Require(glGetError() == GL_NO_ERROR, "Generated PICA draw GL error");
        if (pixel != expected)
            throw std::runtime_error("Generated PICA TEV/logic readback mismatch: " + std::to_string(op));
    }
    if (logic) {
        // Alpha-test NEVER must discard before the wrapper can alter the target.
        const char* name = "PicaFragmentUniforms.fragment_control";
        GLuint index = GL_INVALID_INDEX;
        glGetUniformIndices(program,1,&name,&index);
        Require(index != GL_INVALID_INDEX, "PICA fragment_control not active");
        GLint offset = -1;
        glGetActiveUniformsiv(program,1,&index,GL_UNIFORM_OFFSET,&offset);
        Require(offset >= 0 && offset + 16 <= bytes, "PICA fragment_control offset out of bounds");
        const std::array<GLuint,4> control{0,1,0,0};
        glBufferSubData(GL_UNIFORM_BUFFER,offset,sizeof(control),control.data());
        glUniform1i(operation,0); // CLEAR would erase the destination if discard broke.
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES,0,3);
        std::array<unsigned char,4> pixel{};
        glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        Require(glGetError() == GL_NO_ERROR && pixel == destination,
                "PICA alpha discard did not preserve destination with color logic");
        const std::array<GLuint,4> noAlphaTest{};
        glBufferSubData(GL_UNIFORM_BUFFER,offset,sizeof(noAlphaTest),noAlphaTest.data());
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_GREATER);
        glClearDepthf(0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES,0,3);
        glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        // Zero depth uniforms produce gl_FragDepth=0, so GREATER must fail.
        // Accidentally using raster depth (0.5) would change the destination.
        Require(glGetError() == GL_NO_ERROR && pixel == destination,
                "PICA explicit depth was not preserved with color logic");
        glDisable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearDepthf(1.0f);
    }
    glDeleteBuffers(1, &ubo);
    glDeleteTextures(1, &texture);
}

static void TestGeneratedPicaFragments() {
    constexpr const char* vertex = R"glsl(#version 450
layout(location=0) out vec4 pica_primary_color;
layout(location=1) out vec2 pica_texcoord0;
layout(location=2) out vec2 pica_texcoord1;
layout(location=3) out vec2 pica_texcoord2;
layout(location=4) out float pica_texcoord0_w;
layout(location=5) out vec4 pica_normquat;
layout(location=6) out vec3 pica_view;
void main() {
    pica_primary_color = vec4(0.25,0.5,0.75,1.0);
    pica_texcoord0 = pica_texcoord1 = pica_texcoord2 = vec2(0.5);
    pica_texcoord0_w = 1.0;
    pica_normquat = vec4(0,0,0,1);
    pica_view = vec3(0,0,1);
    const vec2 positions[3] = vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vec4 pica_position = vec4(positions[gl_VertexID],-0.5,1);
    gl_Position = vec4(pica_position.x, pica_position.y, -pica_position.z, pica_position.w);
}
)glsl";
    std::string translatedVertex, error;
    if (!Mm3d::Android::TranslatePicaShaderToGles32(vertex, translatedVertex, &error))
        throw std::runtime_error(error);
    for (auto lighting : Oot3dNativeGame::kOot3dPicaParametricLightings) {
        for (bool integerTexture : {false, true}) {
            Oot3dNativeGame::Oot3dPicaGeneratedFragmentShader generated;
            if (!Oot3dNativeGame::GenerateOot3dPicaParametricBaseFragmentShader(
                    lighting, integerTexture, generated, &error))
                throw std::runtime_error("PICA generation: " + error);
            std::string fragment;
            if (!Mm3d::Android::TranslatePicaShaderToGles32(generated.Source, fragment, &error))
                throw std::runtime_error("Generated PICA translation: " + error);
            try {
                const GLuint program = Program({Shader(GL_VERTEX_SHADER, translatedVertex.c_str()),
                                                Shader(GL_FRAGMENT_SHADER, fragment.c_str())});
                if (lighting == Oot3dNativeGame::Oot3dPicaParametricLighting::Off && !integerTexture)
                    DrawGeneratedPica(program);
                glDeleteProgram(program);
                if (lighting == Oot3dNativeGame::Oot3dPicaParametricLighting::Off && !integerTexture) {
                    std::string logicSource = "stale";
                    Require(!Mm3d::Android::TranslatePicaFragmentWithRgba8Logic(
                                generated.Source, false, logicSource, &error) && logicSource.empty(),
                            "Logic adapter did not reject missing coherent fetch");
                    Require(Mm3d::Android::TranslatePicaFragmentWithRgba8Logic(
                                generated.Source, HasExtension("GL_EXT_shader_framebuffer_fetch"),
                                logicSource, &error), "Generated PICA logic translation failed");
                    const GLuint logicProgram = Program({Shader(GL_VERTEX_SHADER, translatedVertex.c_str()),
                                                         Shader(GL_FRAGMENT_SHADER, logicSource.c_str())});
                    DrawGeneratedPica(logicProgram, true);
                    glDeleteProgram(logicProgram);
                }
            } catch (const std::exception& failure) {
                throw std::runtime_error("PICA lighting=" + std::to_string(static_cast<int>(lighting))
                    + " integer_texture=" + std::to_string(integerTexture) + ": " + failure.what());
            }
        }
    }
    Require(glGetError() == GL_NO_ERROR, "Generated PICA compile/link GL error");
}

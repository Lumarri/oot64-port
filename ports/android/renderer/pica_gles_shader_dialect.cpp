// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_shader_dialect.h"
#include "pica_gles_logic.h"
#include "fast/renderer3ds/pica_gl_shader_dialect.h"

namespace Mm3d::Android {
bool TranslatePicaShaderToGles32(std::string_view canonical, std::string& output,
                               std::string* error) {
    output.clear();
    if (error) error->clear();
    // Extension-bearing variants need explicit ES capability handling before
    // precision declarations can be injected. Fail closed for this first subset.
    if (canonical.find("#extension") != std::string_view::npos) {
        if (error) *error = "GLES PICA extension variants are not yet supported";
        return false;
    }
    std::string desktop;
    // Keep upstream's descriptor-set validation and Vulkan-to-GL depth mapping.
    if (!Fast::Renderer3ds::TranslatePicaShaderToOpenGl43(canonical, desktop, error))
        return false;
    const auto newline = desktop.find('\n');
    output = "#version 320 es\n"
             "precision highp float;\n"
             "precision highp int;\n"
             "precision highp sampler2D;\n"
             "precision highp usampler2D;\n"
             "#line 2\n";
    if (newline != std::string::npos) output.append(desktop.substr(newline + 1));
    return true;
}

bool TranslatePicaFragmentWithRgba8Logic(std::string_view canonical,
                                       bool coherentFramebufferFetch,
                                       std::string& output, std::string* error) {
    output.clear();
    if (error) error->clear();
    const auto reject = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (!coherentFramebufferFetch)
        return reject("RGBA8 logic requires coherent GL_EXT_shader_framebuffer_fetch");
    // This adapter targets the pinned generator, not arbitrary user GLSL.
    if (canonical.find("pica_shadow_buffer") != std::string_view::npos)
        return reject("Shadow image producers do not use RGBA8 color logic");
    if (canonical.find("mm3d_") != std::string_view::npos)
        return reject("Canonical shader collides with GLES adapter identifiers");
    std::string translated;
    if (!TranslatePicaShaderToGles32(canonical, translated, error)) return false;
    constexpr std::string_view declaration = "layout(location=0) out vec4 pica_color;";
    const auto position = translated.find(declaration);
    if (position == std::string::npos ||
        translated.find(declaration, position + declaration.size()) != std::string::npos)
        return reject("Expected the pinned PICA single-color output declaration");
    translated.replace(position, declaration.size(), "layout(location=0) inout vec4 pica_color;");
    const auto newline = translated.find('\n');
    output = "#version 320 es\n#extension GL_EXT_shader_framebuffer_fetch : require\n"
             "#define main mm3d_pica_fragment_main\n";
    output.append(translated.substr(newline + 1));
    output.append("\n#undef main\nuniform int mm3d_logic_operation;\n");
    output.append(PicaRgba8LogicGlsl);
    // Capture destination before the generated main writes its color. A discard
    // inside the generated shader terminates this invocation, including logic.
    output.append(R"glsl(
void main() {
    vec4 mm3d_destination = pica_color;
    mm3d_pica_fragment_main();
    pica_color = pica_logic_rgba8(pica_color, mm3d_destination, mm3d_logic_operation);
}
)glsl");
    return true;
}
}

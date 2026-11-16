// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_shader_dialect.h"
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
}

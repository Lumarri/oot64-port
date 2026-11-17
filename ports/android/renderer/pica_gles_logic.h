// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string_view>

namespace Mm3d::Android {
// PicaLogicOperation numbering from the pinned upstream pica_native_state.h.
// RGBA8 UNORM only. Caller must disable blend/dither and provide the destination
// from coherent EXT_shader_framebuffer_fetch. Other formats need their own masks.
inline constexpr std::string_view PicaRgba8LogicGlsl = R"glsl(
vec4 pica_logic_rgba8(vec4 source, vec4 destination, int operation) {
    uvec4 s = uvec4(floor(clamp(source, 0.0, 1.0) * 255.0 + 0.5));
    uvec4 d = uvec4(floor(clamp(destination, 0.0, 1.0) * 255.0 + 0.5));
    uvec4 r;
    switch (operation) {
    case 0: r = uvec4(0); break;
    case 1: r = s & d; break;
    case 2: r = s & ~d; break;
    case 3: r = s; break;
    case 4: r = uvec4(255); break;
    case 5: r = ~s; break;
    case 6: r = d; break;
    case 7: r = ~d; break;
    case 8: r = ~(s & d); break;
    case 9: r = s | d; break;
    case 10: r = ~(s | d); break;
    case 11: r = s ^ d; break;
    case 12: r = ~(s ^ d); break;
    case 13: r = ~s & d; break;
    case 14: r = s | ~d; break;
    case 15: r = ~s | d; break;
    default: r = s; break;
    }
    return vec4(r & uvec4(255)) / 255.0;
}
)glsl";
}

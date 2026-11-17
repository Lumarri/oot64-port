// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>

namespace Mm3d::Android {
// Initial GLES dialect for canonical PICA sources. Successful translation still
// requires driver compilation; it does not certify every generated PICA variant.
bool TranslatePicaShaderToGles32(std::string_view canonical, std::string& output,
                               std::string* error = nullptr);
// Backend-local variant for the pinned generator's single RGBA8 color output.
// Requires coherent EXT framebuffer fetch, disabled blending/dithering, and
// uniform mm3d_logic_operation set to a valid upstream PicaLogicOperation (0..15).
// Keep this variant separate from ordinary shaders in any backend program cache.
bool TranslatePicaFragmentWithRgba8Logic(std::string_view canonical,
                                       bool coherentFramebufferFetch,
                                       std::string& output, std::string* error = nullptr);
}

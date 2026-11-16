// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>

namespace Mm3d::Android {
// Initial GLES dialect for canonical PICA sources. Successful translation still
// requires driver compilation; it does not certify every generated PICA variant.
bool TranslatePicaShaderToGles32(std::string_view canonical, std::string& output,
                               std::string* error = nullptr);
}

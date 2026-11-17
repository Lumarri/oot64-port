// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "oot3d_native_a32_memory.h"
#include <nlohmann/json_fwd.hpp>

namespace Mm3d::Android {
// Early title execution only: no kernel, scheduler, patches, filesystem or GPU.
// Layout comes from the pinned upstream ExHeader process-manifest builder.
struct InitialTitleState {
    Oot3dNativeGame::NativeA32Memory Memory;
    oot3d::recomp::a32::GuestState Registers{};
};
void InitializeTitleState(InitialTitleState& state, const nlohmann::json& manifest,
                          std::span<const uint8_t> code);
}

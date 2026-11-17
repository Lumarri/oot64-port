// SPDX-License-Identifier: GPL-3.0-or-later
// Load the private title through the real product loader without executing it.
#include "oot3d_native_direct_aot.h"
#include "triaevum_title_plugin_loader.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if (argc!=4) return 2;
    using namespace Oot3dNativeGame;
    try {
        const size_t expected=std::stoull(argv[2]);
        const uint32_t entry=static_cast<uint32_t>(std::stoul(argv[3],nullptr,0));
        ConfigureTitlePlugin(argv[1]);
        if (!Oot3dWholeAotPluginV2Available() || !Oot3dWholeAotPluginObservesMemoryWrites() ||
            !Oot3dWholeAotPluginExitRequestSupported() || Oot3dWholeAotPluginExecutionActive())
            throw std::runtime_error("Title ABI, memory observation or exit capability validation failed");
        const auto entries=Oot3dWholeAotEntryPoints();
        if (entries.size()!=expected || !std::is_sorted(entries.begin(),entries.end()) ||
            std::adjacent_find(entries.begin(),entries.end())!=entries.end() ||
            !std::binary_search(entries.begin(),entries.end(),entry))
            throw std::runtime_error("Title dispatcher inventory does not match the verified contract");
        // The Android loader allows revalidating the same normalized path.
        // Once queried, selecting any different path must remain forbidden.
        ConfigureTitlePlugin(argv[1]);
        bool sealed=false;
        try { ConfigureTitlePlugin(std::filesystem::path(argv[1]).concat(".different")); }
        catch (const std::runtime_error& error) {
            sealed=std::string(error.what())=="Title plugin was already queried";
        }
        if (!sealed) throw std::runtime_error("Title selection was not sealed after ABI query");
        std::cout << "{\"title\":\"mm3d\",\"whole_aot_abi_v2\":true,\"entry_count\":" << entries.size()
                  << ",\"entrypoint_present\":true,\"memory_write_observation\":true,\"exit_request\":true,"
                     "\"same_path_revalidation\":true,\"loader_sealed\":true,\"scope\":\"private_title_loaded_not_game_boot\"}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "MM3D title probe: " << error.what() << '\n'; return 1; }
}

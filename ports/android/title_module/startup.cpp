// SPDX-License-Identifier: GPL-3.0-or-later
// Execute the real private title only as far as its first CTR system call.
#include "initial_state.h"
#include "oot3d_native_direct_aot.h"
#include "triaevum_title_plugin_loader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace {
using namespace Oot3dNativeGame;
namespace a32=oot3d::recomp::a32;
using Json=nlohmann::json;
void Require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
std::vector<uint8_t> Read(const char* path,size_t maximum) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    const auto size=file.tellg();
    Require(file && size>0 && uint64_t(size)<=maximum,"Missing or oversized private startup input");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    Require(bool(file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())),"Incomplete private startup input");
    return bytes;
}
auto Registers(const a32::GuestState& s) {
    return std::tie(s.r,s.cpsr,s.fpscr,s.thread_pointer,s.vfp,s.exclusive_address,
                    s.exclusive_token,s.exclusive_size,s.exclusive_valid);
}
struct Run {
    a32::ExecutionResult Exit{};
    Oot3dWholeAotStats Stats{};
    uint32_t Blocks=0,Calls=0;
    bool FirstBudgetYield=false;
};
Run UntilSvc(Mm3d::Android::InitialTitleState& state,bool sliced) {
    Run run;
    constexpr uint32_t maximum=2000000;
    while (run.Blocks<maximum) {
        const uint32_t budget=sliced?(run.Calls==0?1U:std::min(4096U,maximum-run.Blocks)):maximum;
        uint32_t consumed=0;
        Require(ExecuteOot3dWholeAotFunction(state.Registers.r[15],state.Registers,state.Memory,
            &run.Exit,&run.Stats,nullptr,budget,&consumed,nullptr,nullptr,nullptr,0,nullptr,false,0),
            "Whole-AOT module declined the startup entry");
        Require(consumed>0 && consumed<=budget,"Module exceeded its block budget");
        ++run.Calls; run.Blocks+=consumed;
        Require(!Oot3dWholeAotPluginExecutionActive(),"Module stayed active after returning");
        if (sliced && run.Calls==1) {
            run.FirstBudgetYield=run.Exit.kind==a32::ExitKind::BlockLimit && consumed==1;
            Require(run.FirstBudgetYield,"First startup block did not yield at the requested budget");
        }
        if (run.Exit.kind==a32::ExitKind::Svc) return run;
        if (run.Exit.kind!=a32::ExitKind::BlockLimit)
            throw std::runtime_error("Startup stopped before SVC: kind="+std::to_string(unsigned(run.Exit.kind))+
                " pc="+std::to_string(run.Exit.pc)+" detail="+std::to_string(run.Exit.detail));
    }
    throw std::runtime_error("Startup exhausted its total block budget before the first SVC");
}
}
int main(int argc,char** argv) {
    if (argc!=4) return 2;
    try {
        ConfigureTitlePlugin(argv[1]);
        Require(Oot3dWholeAotPluginV2Available() && Oot3dWholeAotPluginObservesMemoryWrites(),"Required title ABI unavailable");
        const auto code=Read(argv[2],8*1024*1024),encoded=Read(argv[3],128*1024);
        const auto manifest=Json::parse(encoded.begin(),encoded.end());
        const uint32_t entry=manifest.at("process").at("entrypoint").get<uint32_t>();
        const auto entries=Oot3dWholeAotEntryPoints();
        Require(std::binary_search(entries.begin(),entries.end(),entry),"Startup entry is absent from the module");
        Mm3d::Android::InitialTitleState continuous,sliced;
        Mm3d::Android::InitializeTitleState(continuous,manifest,code);
        Mm3d::Android::InitializeTitleState(sliced,manifest,code);
        const auto generation=continuous.Memory.WriteGeneration();
        const auto first=UntilSvc(continuous,false),resumed=UntilSvc(sliced,true);
        Require(first.Exit.pc==resumed.Exit.pc && first.Exit.detail==resumed.Exit.detail &&
                Registers(continuous.Registers)==Registers(sliced.Registers) &&
                continuous.Memory.ContentFingerprint()==sliced.Memory.ContentFingerprint(),
                "Startup state differs after budgeted resume");
        uint32_t instruction=0;
        Require(continuous.Memory.Read32(first.Exit.pc,&instruction) &&
                (instruction&0xFF000000U)==0xEF000000U && (instruction&0x00FFFFFFU)==first.Exit.detail,
                "Reported SVC does not match the verified ARM instruction");
        Require(continuous.Memory.WriteGeneration()>generation && first.Stats.SvcExits==1 &&
                resumed.Stats.SvcExits==1 && first.Stats.MemoryFaults==0 && resumed.Stats.MemoryFaults==0 &&
                first.Stats.UnsupportedExits==0 && resumed.Stats.UnsupportedExits==0 &&
                first.Stats.ExternalCalls==0 && resumed.Stats.ExternalCalls==0,
                "Startup produced unexpected faults, fallbacks or missing memory writes");
        std::cout<<Json{{"title","mm3d"},{"scope","real_title_entry_to_first_svc_not_game_boot"},
            {"entrypoint",entry},{"svc_pc",first.Exit.pc},{"svc_immediate",first.Exit.detail},
            {"continuous_blocks",first.Blocks},{"sliced_blocks",resumed.Blocks},{"resume_calls",resumed.Calls},
            {"mapped_regions",continuous.Memory.RegionCount()},
            {"writes",continuous.Memory.WriteGeneration()-generation},
            {"budget_yield",resumed.FirstBudgetYield},{"resume_state_matches",true},
            {"svc_instruction_matches",true},{"memory_faults",0},{"unsupported_exits",0},
            {"guest_services_executed",false}}.dump()<<'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"MM3D startup probe: "<<error.what()<<'\n';
        return 1;
    }
}

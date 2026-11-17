// SPDX-License-Identifier: GPL-3.0-or-later
#include "initial_state.h"
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>

namespace Mm3d::Android {
namespace {
using namespace Oot3dNativeGame;
uint32_t U32(const nlohmann::json& value) {
    if (!value.is_number_unsigned() || value.get<uint64_t>()>UINT32_MAX)
        throw std::runtime_error("Invalid unsigned process-layout field");
    return value.get<uint32_t>();
}
void Require(bool valid,const char* message) {
    if (!valid) throw std::runtime_error(message);
}
void Map(NativeA32Memory& memory,const NativeA32MemoryRegionConfig& region) {
    std::string error;
    Require(region.BaseAddress%4096==0 && region.Size>0 && region.Size%4096==0,
            "Unaligned initial mapping");
    if (!memory.MapRegion(region,&error)) throw std::runtime_error(error);
}
}
void InitializeTitleState(InitialTitleState& state,const nlohmann::json& manifest,
                          std::span<const uint8_t> code) {
    Require(state.Memory.RegionCount()==0,"Initial title state is already mapped");
    Require(manifest.at("format")=="oot3d_native_process_manifest_v1","Unsupported process layout");
    Require(U32(manifest.at("source").at("code_bin_size"))==code.size(),"Code size differs from layout");
    const auto& process=manifest.at("process");
    Require(U32(process.at("page_size"))==4096,"Unexpected CTR page size");
    Require(process.at("system_regions").size()<=16 && process.at("segments").size()==3,
            "Unexpected initial region inventory");
    for (const auto& region: process.at("system_regions")) {
        const uint32_t address=U32(region.at("address")),size=U32(region.at("mapped_size"));
        Require(size<=0x800000,"Oversized initial system region");
        std::vector<uint8_t> initial(size,0);
        for (const auto& item: region.at("initial_values")) {
            const uint32_t offset=U32(item.at("offset")),width=U32(item.at("size"));
            Require((width==1 || width==2 || width==4 || width==8) && offset<=size && width<=size-offset,
                    "Invalid initial system value");
            const uint64_t value=item.at("value").get<uint64_t>();
            Require(width==8 || value<(uint64_t{1}<<(8*width)),"System value exceeds field width");
            for (unsigned i=0;i<width;++i) initial[offset+i]=uint8_t(value>>(8*i));
        }
        const bool device=address==0x1F000000U || address==0x1FF00000U;
        Map(state.Memory,{region.at("name").get<std::string>(),address,size,
            region.at("writable").get<bool>(),region.at("executable").get<bool>(),initial,
            device?NativeA32KernelMemoryState::Io:NativeA32KernelMemoryState::Shared});
    }
    for (const auto& segment: process.at("segments")) {
        const uint32_t offset=U32(segment.at("file_offset")),size=U32(segment.at("file_size"));
        const uint32_t mapped=U32(segment.at("mapped_size"));
        Require(offset<=code.size() && size<=code.size()-offset && size<=mapped && mapped<=0x2000000,
                "Invalid initial code segment");
        const bool writable=segment.at("writable").get<bool>();
        Map(state.Memory,{segment.at("name").get<std::string>(),U32(segment.at("address")),mapped,
            writable,segment.at("executable").get<bool>(),code.subspan(offset,size),
            writable?NativeA32KernelMemoryState::Private:NativeA32KernelMemoryState::Code});
    }
    const auto& thread=manifest.at("primary_thread");
    const uint32_t stack=U32(thread.at("stack_base_address")),stackSize=U32(thread.at("stack_size"));
    const uint32_t tls=U32(thread.at("tls_base_address")),tlsSize=U32(thread.at("tls_size"));
    Require(stackSize<=0x800000 && tlsSize==4096 && uint64_t(stack)+stackSize<=UINT32_MAX,
            "Invalid initial stack/TLS");
    Map(state.Memory,{"primary_stack",stack,stackSize,true,false,{},NativeA32KernelMemoryState::Locked});
    Map(state.Memory,{"primary_tls",tls,tlsSize,true,false,{},NativeA32KernelMemoryState::Locked});
    auto& registers=state.Registers;
    registers={};
    registers.r[0]=U32(thread.at("argument0"));
    registers.r[13]=(stack+stackSize)&~7U;
    registers.r[15]=U32(process.at("entrypoint"));
    registers.cpsr=U32(thread.at("initial_cpsr"));
    registers.fpscr=U32(thread.at("initial_fpscr"));
    registers.thread_pointer=U32(thread.at("thread_pointer"));
    Require(state.Memory.IsExecutable(registers.r[15],4) &&
            state.Memory.IsWritable(registers.thread_pointer,512),"Invalid initial PC or thread pointer");
}
}

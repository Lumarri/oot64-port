// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_draw.h"
#include "fast/extension/pica_uniform_layout.h"
#include "fast/renderer3ds/pica_tev_program.h"
#include <cstring>

static void TestPicaDrawContract() {
    using namespace Fast::Renderer3ds;
    Mm3d::Android::GlesRenderTarget target;
    Mm3d::Android::PicaGlesDraw renderer;
    std::string error;
    Require(target.Resize(64,64,&error),"Draw contract target allocation failed");
    constexpr const char* vertex = R"glsl(#version 450
layout(location=0) in vec4 pica_input;
layout(set=0,binding=0,std140) uniform VertexUniforms { vec4 tint; } uniforms;
layout(location=0) out vec4 primary;
void main() {
    primary = uniforms.tint;
    vec4 pica_position = pica_input;
    gl_Position = vec4(pica_position.x, pica_position.y, -pica_position.z, pica_position.w);
}
)glsl";
    constexpr const char* fragment = R"glsl(#version 450
layout(location=0) in vec4 primary;
layout(set=0,binding=4,std140) uniform FragmentUniforms { vec4 multiplier; } uniforms;
layout(location=0) out vec4 pica_color;
void main() { pica_color = primary * uniforms.multiplier; }
)glsl";
    const std::array<float,12> positions{-1,-1,-0.5f,1, 3,-1,-0.5f,1, -1,3,-0.5f,1};
    std::array<float,4> tint{1,0,0,1}, multiplier{1,1,1,1};
    const auto bytes = [](const auto& values) {
        return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(values.data()),sizeof(values));
    };
    PicaVertexBindingView binding{0,16,false,bytes(positions)};
    PicaVertexAttributeView attribute{0,0,PicaVertexFormat::Float,4,0};
    PicaDrawView draw;
    draw.VertexShaderSource=vertex; draw.FragmentShaderSource=fragment;
    draw.VertexBindings={&binding,1}; draw.VertexAttributes={&attribute,1};
    draw.VertexUniformBytes=bytes(tint); draw.FragmentUniformBytes=bytes(multiplier);
    draw.VertexCount=3;
    draw.FramebufferWidth=draw.FramebufferHeight=64;
    draw.FramebufferColorFormat=0; draw.FramebufferDepthFormat=3;
    draw.ViewportWidth=draw.ViewportHeight=64;
    draw.ColorWriteMask=15;
    const auto clear = [&] {
        glBindFramebuffer(GL_FRAMEBUFFER,target.Framebuffer());
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glDepthMask(GL_TRUE); glStencilMask(255);
        glClearColor(0,0,1,1); glClearDepthf(0.75f); glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    };
    const auto submit = [&] {
        if (!renderer.Submit(draw,target,&error)) throw std::runtime_error("Draw contract: "+error);
    };
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    Require(renderer.CachedPrograms()==1,"First PICA program not cached");
    draw.LogicOperation=PicaLogicOperation::Xor;
    clear(); submit(); TargetPixel(32,32,{255,0,255,0});
    Require(renderer.CachedPrograms()==2,"Logic and ordinary PICA shaders share a cache entry");
    draw.Blend.Enabled=true; // Blending selects the ordinary shader even with XOR in the state.
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    Require(renderer.CachedPrograms()==2,"Returning to normal PICA unnecessarily recompiled");
    draw.Blend.SourceRgb=NativeBlendFactor::SourceAlpha;
    draw.Blend.DestRgb=NativeBlendFactor::OneMinusSourceAlpha;
    tint[3]=0.5f;
    clear(); submit();
    std::array<unsigned char,4> mixed{};
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,mixed.data());
    Require(mixed[0]>=127 && mixed[0]<=128 && mixed[1]==0 && mixed[2]>=127 && mixed[2]<=128
            && mixed[3]>=127 && mixed[3]<=128,"PICA source-alpha blending mismatch");
    tint[3]=1;
    draw.Blend.Enabled=false; draw.LogicOperation=PicaLogicOperation::Copy;
    draw.ScissorMode=3; draw.ScissorX2=31; draw.ScissorY2=63;
    clear(); submit(); TargetPixel(16,32,{255,0,0,255}); TargetPixel(48,32,{0,0,255,255});
    draw.ScissorMode=0;
    draw.CullMode=NativeCullMode::KeepClockwise;
    clear(); submit(); TargetPixel(32,32,{0,0,255,255});
    draw.CullMode=NativeCullMode::KeepCounterClockwise;
    submit(); TargetPixel(32,32,{255,0,0,255});
    draw.CullMode=NativeCullMode::KeepAll;
    draw.ColorWriteMask=2;
    tint={0,1,0,1};
    clear(); submit(); TargetPixel(32,32,{0,255,255,255});
    draw.ColorWriteMask=15;
    draw.DepthTestEnabled=true; draw.DepthCompare=PicaCompareFunction::Greater;
    clear(); submit(); TargetPixel(32,32,{0,0,255,255});
    draw.DepthCompare=PicaCompareFunction::Less; draw.DepthWriteEnabled=true;
    submit(); TargetPixel(32,32,{0,255,0,255});
    tint={1,0,0,1};
    submit(); TargetPixel(32,32,{0,255,0,255}); // Equal depth rejected after previous write.
    draw.DepthTestEnabled=false;
    clear(); submit(); // Depth writes with compare disabled must use ALWAYS.
    draw.DepthTestEnabled=true; tint={0,1,0,1};
    submit(); TargetPixel(32,32,{255,0,0,255});
    draw.DepthTestEnabled=draw.DepthWriteEnabled=false;
    draw.Stencil.Enabled=true; draw.Stencil.Compare=PicaCompareFunction::Always;
    draw.Stencil.Reference=1; draw.Stencil.CompareMask=draw.Stencil.WriteMask=255;
    draw.Stencil.Pass=PicaStencilAction::Replace;
    clear(); submit();
    draw.Stencil.Compare=PicaCompareFunction::Equal; draw.Stencil.Reference=0;
    tint={1,0,0,1};
    submit(); TargetPixel(32,32,{0,255,0,255});
    draw.Stencil.Enabled=false;
    const std::array<float,16> indexedPositions{0,0,0,1, -1,-1,-0.5f,1, 3,-1,-0.5f,1, -1,3,-0.5f,1};
    const std::array<uint8_t,3> indices8{0,1,2};
    const std::array<uint16_t,3> indices16{0,1,2};
    binding.Bytes=bytes(indexedPositions);
    draw.Indexed=true; draw.BaseVertex=1; draw.IndexBytes=bytes(indices8);
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    draw.IndicesAre16Bit=true; draw.IndexBytes=bytes(indices16);
    draw.Topology=PicaTopology::TriangleStrip;
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    draw.Topology=PicaTopology::TriangleFan;
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    const auto rejected = [&] {
        clear();
        Require(!renderer.Submit(draw,target,&error) && !error.empty(),"Unsupported/malformed draw was accepted");
        TargetPixel(32,32,{0,0,255,255});
    };
    draw.BaseVertex=2; rejected(); // Vertex past the end of the uploaded array.
    draw.BaseVertex=-1; rejected();
    draw.BaseVertex=1;
    draw.FragmentUniformBytes={}; rejected(); draw.FragmentUniformBytes=bytes(multiplier);
    attribute.Location=1; rejected(); attribute.Location=0;
    draw.ScissorMode=1; rejected(); draw.ScissorMode=0;
    draw.FramebufferColorFormat=2; rejected(); draw.FramebufferColorFormat=0;
    draw.Topology=PicaTopology::GeometryShader; rejected(); draw.Topology=PicaTopology::TriangleList;
    const PicaTextureView texture{};
    draw.Textures={&texture,1}; rejected(); draw.Textures={};
    // Reflection must also reject a sampler hidden in otherwise valid shaders.
    draw.FragmentShaderSource=R"glsl(#version 450
layout(set=0,binding=1) uniform sampler2D unsupported_texture;
layout(location=0) out vec4 pica_color;
void main() { pica_color = texture(unsupported_texture,vec2(0.5)); }
)glsl";
    rejected(); draw.FragmentShaderSource=fragment;
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    // Exercise the upstream tiled-texture decoder through the draw contract.
    draw.FragmentShaderSource=R"glsl(#version 450
layout(set=0,binding=1) uniform sampler2D pica_texture0;
layout(set=0,binding=4,std140) uniform FragmentUniforms { vec4 uv; } uniforms;
layout(location=0) out vec4 pica_color;
void main() { pica_color = texture(pica_texture0,gl_FragCoord.xy / 64.0 * uniforms.uv.xy + uniforms.uv.zw); }
)glsl";
    multiplier={1,1,0,0};
    std::array<uint8_t,256> tiled{};
    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
        unsigned morton=0;
        for (unsigned bit=0;bit<3;++bit)
            morton |= ((x>>bit)&1)<<(2*bit) | ((y>>bit)&1)<<(2*bit+1);
        tiled[morton*4]=255; tiled[morton*4+1]=64;
        tiled[morton*4+2]=static_cast<uint8_t>(y*32);
        tiled[morton*4+3]=static_cast<uint8_t>(x*32);
    }
    PicaTextureView sampled;
    sampled.Width=sampled.Height=8; sampled.NativeBytes=tiled;
    sampled.NativeContentHashAvailable=true; sampled.NativeContentHash=99;
    draw.Textures={&sampled,1};
    clear(); submit(); TargetPixel(4,4,{0,0,64,255}); TargetPixel(60,60,{224,224,64,255});
    multiplier[0]=2;
    sampled.NativeWrapS=2;
    clear(); submit(); TargetPixel(48,32,{128,128,64,255});
    sampled.NativeWrapS=3;
    clear(); submit(); TargetPixel(48,32,{96,128,64,255});
    sampled.NativeWrapS=0;
    clear(); submit(); TargetPixel(48,32,{224,128,64,255});
    multiplier={0,0,0.5f,0.5f};
    sampled.MinLinear=sampled.MagLinear=true;
    clear(); submit(); TargetPixel(32,32,{112,112,64,255});
    for (size_t i=3;i<tiled.size();i+=4) tiled[i]=255;
    clear(); submit(); TargetPixel(32,32,{255,112,64,255}); // Changed bytes, same identity/hash.
    sampled.MinLinear=sampled.MagLinear=false;
    // Packed RGB565, RGB5A1, RGBA4 and ETC1 families use the same upload path.
    struct Packed { uint8_t format, lo, hi; std::array<unsigned char,4> color; };
    for (const auto& packed : {Packed{3,0xE0,0x07,{0,255,0,255}},
                              Packed{2,0x3F,0,{0,0,255,255}},
                              Packed{4,0x0F,0xFF,{255,255,0,255}}}) {
        std::array<uint8_t,128> encoded{};
        for (size_t i=0;i<encoded.size();i+=2) { encoded[i]=packed.lo; encoded[i+1]=packed.hi; }
        sampled.NativeFormat=packed.format; sampled.NativeBytes=encoded;
        clear(); submit(); TargetPixel(32,32,packed.color);
    }
    std::array<uint8_t,64> etc{};
    sampled.NativeFormat=12; sampled.NativeBytes=std::span<const uint8_t>(etc).first(32);
    clear(); submit(); TargetPixel(32,32,{2,2,2,255});
    sampled.NativeFormat=13; sampled.NativeBytes=etc;
    clear(); submit(); TargetPixel(32,32,{2,2,2,0});
    sampled.NativeFormat=0; sampled.NativeBytes={}; rejected(); sampled.NativeBytes=tiled;
    sampled.NativeWrapS=1; rejected(); sampled.NativeWrapS=0;
    sampled.MaxMipLevel=1; rejected(); sampled.MaxMipLevel=0;
    sampled.NativeType=2; rejected(); sampled.NativeType=0;
    const PicaTextureCopyReference copy{};
    sampled.CopyReferences={&copy,1}; rejected(); sampled.CopyReferences={};
    draw.Textures={}; rejected();
    // End-to-end controlled packet: real upstream fragment generator + packed
    // uniforms + tiled native texture + indexed geometry through Submit().
    Oot3dNativeGame::Oot3dPicaGeneratedFragmentShader generated;
    Require(Oot3dNativeGame::GenerateOot3dPicaParametricBaseFragmentShader(
        Oot3dNativeGame::Oot3dPicaParametricLighting::Off,false,generated,&error),"Packet fragment generation failed");
    std::array<uint8_t,Fast::Extension::kPicaPackedFragmentUniformSize> packedUniforms{};
    const auto word = [&](size_t offset,uint32_t value) { std::memcpy(packedUniforms.data()+offset,&value,4); };
    PicaTevProgram tev;
    for (auto& stage : tev.Stages) stage[0]=3u|(3u<<16); // Texture 0, RGB and alpha.
    tev.Control[1]=1; // TEV effective texture-reference mask.
    std::memcpy(packedUniforms.data()+Fast::Extension::kPicaPackedFragmentTevProgramOffset,&tev,sizeof(tev));
    word(Fast::Extension::kPicaPackedFragmentControlOffset+8,1); // Enable texture 0.
    std::array<PicaTextureView,3> nativeTextures{sampled,sampled,sampled};
    for (unsigned slot=0;slot<3;++slot) nativeTextures[slot].Slot=static_cast<uint8_t>(slot);
    draw.Textures=nativeTextures;
    draw.FragmentUniformBytes=packedUniforms;
    draw.FragmentShaderSource=generated.Source;
    draw.VertexShaderSource=R"glsl(#version 450
layout(location=0) in vec4 pica_input;
layout(location=0) out vec4 pica_primary_color;
layout(location=1) out vec2 pica_texcoord0;
layout(location=2) out vec2 pica_texcoord1;
layout(location=3) out vec2 pica_texcoord2;
layout(location=4) out float pica_texcoord0_w;
layout(location=5) out vec4 pica_normquat;
layout(location=6) out vec3 pica_view;
void main() {
    pica_primary_color=vec4(1,0,0,1);
    pica_texcoord0=pica_texcoord1=pica_texcoord2=vec2(0.5);
    pica_texcoord0_w=1.0; pica_normquat=vec4(0,0,0,1); pica_view=vec3(0,0,1);
    vec4 pica_position=pica_input;
    gl_Position = vec4(pica_position.x, pica_position.y, -pica_position.z, pica_position.w);
}
)glsl";
    clear(); submit(); TargetPixel(32,32,{255,128,64,255});
    draw.Textures={}; draw.VertexShaderSource=vertex; draw.FragmentShaderSource=fragment;
    multiplier={1,1,1,1}; draw.FragmentUniformBytes=bytes(multiplier);
    renderer.Release();
    Require(renderer.CachedPrograms()==0,"Renderer release retained cached programs");
    clear(); submit(); TargetPixel(32,32,{255,0,0,255});
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    TestPicaBackend(draw);
    TestPicaTransfers(draw);
    TestContextRecreation(draw);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
}

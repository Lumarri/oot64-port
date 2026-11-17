// SPDX-License-Identifier: GPL-3.0-or-later
#include "pica_gles_draw.h"
#include "pica_gles_shader_dialect.h"
#include "oot3d/renderer/pica_texture_decode.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Mm3d::Android {
namespace {
using namespace Fast::Renderer3ds;
void Check(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
constexpr GLenum compares[] = {GL_NEVER,GL_ALWAYS,GL_EQUAL,GL_NOTEQUAL,GL_LESS,GL_LEQUAL,GL_GREATER,GL_GEQUAL};
constexpr GLenum stencilOps[] = {GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,GL_INCR_WRAP,GL_DECR_WRAP};
constexpr GLenum equations[] = {GL_FUNC_ADD,GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT,GL_MIN,GL_MAX};
constexpr GLenum factors[] = {GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,
    GL_CONSTANT_COLOR,GL_ONE_MINUS_CONSTANT_COLOR,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA,GL_SRC_ALPHA_SATURATE};
template<class T, size_t N> GLenum Map(T value, const GLenum (&values)[N]) {
    const auto index = static_cast<unsigned>(value);
    Check(index < N, "Invalid PICA raster state enum");
    return values[index];
}
bool CoherentFetch() {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS,&count);
    for (GLint i=0;i<count;++i)
        if (std::strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,i)),
                        "GL_EXT_shader_framebuffer_fetch") == 0) return true;
    return false;
}
GLenum TextureWrap(uint8_t wrap) {
    switch (wrap) {
    case 0: return GL_CLAMP_TO_EDGE;
    case 2: case 6: case 7: return GL_REPEAT;
    case 3: return GL_MIRRORED_REPEAT;
    default: throw std::runtime_error("Border/asymmetric texture wrapping is not yet supported");
    }
}
GLuint Compile(GLenum stage, const std::string& source) {
    GLuint shader = glCreateShader(stage);
    const char* text = source.c_str();
    glShaderSource(shader,1,&text,nullptr);
    glCompileShader(shader);
    GLint okay = 0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);
    if (!okay) {
        std::array<char,4096> log{};
        glGetShaderInfoLog(shader,log.size(),nullptr,log.data());
        glDeleteShader(shader);
        throw std::runtime_error(std::string("PICA GLES compile: ")+log.data());
    }
    return shader;
}
GLuint BuildProgram(const PicaDrawView& draw, bool logic) {
    std::string vertex, fragment, error;
    if (!TranslatePicaShaderToGles32(draw.VertexShaderSource,vertex,&error) ||
        !(logic ? TranslatePicaFragmentWithRgba8Logic(draw.FragmentShaderSource,CoherentFetch(),fragment,&error)
                : TranslatePicaShaderToGles32(draw.FragmentShaderSource,fragment,&error)))
        throw std::runtime_error(error);
    GLuint vs=0,fs=0,program=0;
    try {
        vs=Compile(GL_VERTEX_SHADER,vertex);
        fs=Compile(GL_FRAGMENT_SHADER,fragment);
        program=glCreateProgram();
        glAttachShader(program,vs);
        glAttachShader(program,fs);
        glLinkProgram(program);
        GLint linked=0;
        glGetProgramiv(program,GL_LINK_STATUS,&linked);
        if (!linked) {
            std::array<char,4096> log{};
            glGetProgramInfoLog(program,log.size(),nullptr,log.data());
            throw std::runtime_error(std::string("PICA GLES link: ")+log.data());
        }
        glDetachShader(program,vs);
        glDetachShader(program,fs);
        glDeleteShader(vs); glDeleteShader(fs);
        return program;
    } catch (...) {
        glDeleteProgram(program); glDeleteShader(vs); glDeleteShader(fs);
        throw;
    }
}
void ValidateUniforms(GLuint program, const PicaDrawView& draw) {
    GLint count=0;
    glGetProgramiv(program,GL_ACTIVE_UNIFORM_BLOCKS,&count);
    for (GLint i=0;i<count;++i) {
        GLint binding=0,bytes=0;
        glGetActiveUniformBlockiv(program,i,GL_UNIFORM_BLOCK_BINDING,&binding);
        glGetActiveUniformBlockiv(program,i,GL_UNIFORM_BLOCK_DATA_SIZE,&bytes);
        Check(binding==0 || binding==4,"Only PICA UBO bindings 0 and 4 are supported");
        const auto data = binding==0 ? draw.VertexUniformBytes : draw.FragmentUniformBytes;
        Check(bytes>=0 && data.size()>=static_cast<size_t>(bytes),"PICA uniform payload is too short");
    }
    glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&count);
    for (GLuint i=0;i<static_cast<GLuint>(count);++i) {
        GLint block=0;
        glGetActiveUniformsiv(program,1,&i,GL_UNIFORM_BLOCK_INDEX,&block);
        if (block<0) {
            std::array<char,128> name{};
            GLsizei length=0; GLint size=0; GLenum type=0;
            glGetActiveUniform(program,i,name.size(),&length,&size,&type,name.data());
            if (type==GL_SAMPLER_2D && size==1) {
                GLint unit=0;
                glGetUniformiv(program,glGetUniformLocation(program,name.data()),&unit);
                Check(unit>=1 && unit<=3 && std::any_of(draw.Textures.begin(),draw.Textures.end(),
                    [unit](const auto& texture) { return texture.Slot==unit-1; }),
                    "Active PICA sampler has no texture at binding 1, 2 or 3");
            } else Check(std::string_view(name.data())=="mm3d_logic_operation" && type==GL_INT && size==1,
                         "Unsupported standalone uniform or sampler type");
        }
    }
}
void ValidateInputs(GLuint program, const std::array<bool,16>& locations) {
    GLint count=0;
    glGetProgramiv(program,GL_ACTIVE_ATTRIBUTES,&count);
    for (GLuint i=0;i<static_cast<GLuint>(count);++i) {
        std::array<char,256> name{};
        GLsizei length=0; GLint size=0; GLenum type=0;
        glGetActiveAttrib(program,i,name.size(),&length,&size,&type,name.data());
        const GLint location=glGetAttribLocation(program,name.data());
        Check(location>=0 && location<16 && locations[location],"Shader attribute has no PICA vertex binding");
        Check(size==1 && (type==GL_FLOAT || type==GL_FLOAT_VEC2 || type==GL_FLOAT_VEC3 || type==GL_FLOAT_VEC4),
              "Only scalar/vector floating-point shader inputs are supported");
    }
}
}

void PicaGlesDraw::Release() {
    for (const auto& item : programs_) glDeleteProgram(item.second);
    if (vao_) glDeleteVertexArrays(1,&vao_);
    if (indices_) glDeleteBuffers(1,&indices_);
    for (GLuint buffer : vertices_) if (buffer) glDeleteBuffers(1,&buffer);
    for (GLuint buffer : uniforms_) if (buffer) glDeleteBuffers(1,&buffer);
    for (GLuint texture : textures_) if (texture) glDeleteTextures(1,&texture);
    AbandonContext();
}

void PicaGlesDraw::AbandonContext() {
    programs_.clear();
    vao_=indices_=0; vertices_.fill(0); uniforms_.fill(0); textures_.fill(0);
}

bool PicaGlesDraw::Submit(const PicaDrawView& draw, GlesRenderTarget& target, std::string* error) {
    if (error) error->clear();
    try {
        Check(target.Framebuffer() && target.Width()==draw.FramebufferWidth && target.Height()==draw.FramebufferHeight,
              "PICA target extent mismatch");
        Check(draw.FramebufferColorFormat==0 && draw.FramebufferDepthFormat==3,
              "Initial GLES draw path requires RGBA8 and depth24/stencil8");
        Check(draw.FragmentOperationMode==0 && draw.LightingLut.PackedEntries.empty(),
              "Lighting LUT uploads and shadow producers are not yet supported");
        std::array<std::vector<uint8_t>,3> decodedTextures;
        std::array<bool,3> textureSlots{};
        for (const auto& texture : draw.Textures) {
            Check(texture.Slot<3 && !textureSlots[texture.Slot],"Invalid or duplicate texture slot");
            textureSlots[texture.Slot]=true;
            Check(texture.NativeType==0 && texture.CopyReferences.empty(),"Only CPU-owned ordinary 2D textures are supported");
            Check(texture.Width>0 && texture.Height>0 && texture.Width<=1024 && texture.Height<=1024,
                  "Initial streaming textures must have dimensions 1..1024");
            Check(texture.MinMipLevel==0 && texture.MaxMipLevel==0 && texture.LodBiasRaw==0,
                  "Mip chains and LOD bias are not yet supported");
            TextureWrap(texture.NativeWrapS); TextureWrap(texture.NativeWrapT);
            std::string decodeError;
            if (!Oot3d::Renderer::DecodePicaTextureRgba8(texture.NativeFormat,texture.Width,texture.Height,
                    texture.NativeBytes,decodedTextures[texture.Slot],&decodeError))
                throw std::runtime_error("PICA texture decode: "+decodeError);
        }
        Check(draw.ScissorMode==0 || draw.ScissorMode==3,"Unsupported PICA scissor mode");
        Check(draw.UiFrameExtensionY==1.0f,"UI frame expansion is not yet supported");
        Check(!draw.VertexShaderSource.empty() && !draw.FragmentShaderSource.empty(),"Missing canonical shaders");
        Check(draw.VertexCount>0 && !draw.VertexBindings.empty() && !draw.VertexAttributes.empty(),"Missing PICA geometry");
        Check(static_cast<unsigned>(draw.Topology)<3 && static_cast<unsigned>(draw.CullMode)<3 &&
              static_cast<unsigned>(draw.LogicOperation)<16 && draw.ColorWriteMask<=15,"Unsupported PICA draw state");
        const GLenum topology[] = {GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN};
        const auto depth=Map(draw.DepthCompare,compares), stencil=Map(draw.Stencil.Compare,compares);
        const auto sf=Map(draw.Stencil.Fail,stencilOps), sd=Map(draw.Stencil.DepthFail,stencilOps), sp=Map(draw.Stencil.Pass,stencilOps);
        const auto er=Map(draw.Blend.EquationRgb,equations), ea=Map(draw.Blend.EquationAlpha,equations);
        const auto sr=Map(draw.Blend.SourceRgb,factors), dr=Map(draw.Blend.DestRgb,factors);
        const auto sa=Map(draw.Blend.SourceAlpha,factors), da=Map(draw.Blend.DestAlpha,factors);
        Check(dr!=GL_SRC_ALPHA_SATURATE && sa!=GL_SRC_ALPHA_SATURATE && da!=GL_SRC_ALPHA_SATURATE,
              "Unsupported saturate blend factor position");
        for (float value : {draw.ViewportX,draw.ViewportY,draw.ViewportWidth,draw.ViewportHeight})
            Check(std::isfinite(value) && value>=-32768.f && value<=32767.f,"Invalid PICA viewport");
        Check(draw.ViewportWidth>0 && draw.ViewportHeight>0,"Empty PICA viewport");
        std::array<const PicaVertexBindingView*,16> bindings{};
        for (const auto& b : draw.VertexBindings) {
            Check(b.Binding<16 && !bindings[b.Binding] && b.ByteStride && !b.PerInstance,
                  "Unsupported or duplicate vertex binding");
            bindings[b.Binding]=&b;
        }
        size_t count=draw.VertexCount;
        int64_t largest=static_cast<int64_t>(count)-1;
        if (draw.Indexed) {
            const size_t stride=draw.IndicesAre16Bit?2:1;
            Check(!draw.IndexBytes.empty() && draw.IndexBytes.size()%stride==0,"Invalid index payload");
            count=draw.IndexBytes.size()/stride;
            largest=-1;
            for (size_t i=0;i<count;++i) {
                uint16_t index=draw.IndexBytes[i*stride];
                if (stride==2) std::memcpy(&index,draw.IndexBytes.data()+i*stride,2);
                const int64_t adjusted=static_cast<int64_t>(index)+draw.BaseVertex;
                Check(adjusted>=0,"Index plus base vertex is negative");
                largest=std::max(largest,adjusted);
            }
        } else Check(draw.BaseVertex==0,"Base vertex requires indexed geometry");
        Check(count<=static_cast<size_t>(std::numeric_limits<GLsizei>::max()),"Too many vertices/indices");
        std::array<bool,16> locations{};
        constexpr size_t scalarBytes[]={1,1,2,4};
        constexpr GLenum scalarTypes[]={GL_BYTE,GL_UNSIGNED_BYTE,GL_SHORT,GL_FLOAT};
        for (const auto& a : draw.VertexAttributes) {
            Check(a.Location<16 && !locations[a.Location] && a.Binding<16 && bindings[a.Binding] &&
                  a.ComponentCount>=1 && a.ComponentCount<=4 && static_cast<unsigned>(a.Format)<4,
                  "Invalid PICA vertex attribute");
            locations[a.Location]=true;
            const auto& b=*bindings[a.Binding];
            const size_t end=a.ByteOffset+a.ComponentCount*scalarBytes[static_cast<unsigned>(a.Format)];
            Check(end<=b.ByteStride && static_cast<uint64_t>(largest)*b.ByteStride+end<=b.Bytes.size(),
                  "Vertex/index data would read beyond the uploaded buffer");
        }
        Check(glGetError()==GL_NO_ERROR,"Pre-existing GLES error before PICA draw");
        const bool logic=!draw.Blend.Enabled && draw.LogicOperation!=PicaLogicOperation::Copy;
        ProgramKey key{std::string(draw.VertexShaderSource),std::string(draw.FragmentShaderSource),logic};
        auto found=programs_.find(key);
        if (found==programs_.end()) {
            Check(programs_.size()<64,"Initial GLES program cache limit reached");
            GLuint program=BuildProgram(draw,logic);
            try {
                ValidateUniforms(program,draw);
                ValidateInputs(program,locations);
                found=programs_.emplace(std::move(key),program).first;
            }
            catch (...) { glDeleteProgram(program); throw; }
        } else {
            ValidateUniforms(found->second,draw);
            ValidateInputs(found->second,locations);
        }
        const GLuint program=found->second;
        if (!vao_) {
            glGenVertexArrays(1,&vao_); glGenBuffers(1,&indices_);
            glGenBuffers(vertices_.size(),vertices_.data()); glGenBuffers(uniforms_.size(),uniforms_.data());
            glGenTextures(textures_.size(),textures_.data());
        }
        glBindVertexArray(vao_);
        for (GLuint i=0;i<16;++i) glDisableVertexAttribArray(i);
        for (const auto& b : draw.VertexBindings) {
            glBindBuffer(GL_ARRAY_BUFFER,vertices_[b.Binding]);
            glBufferData(GL_ARRAY_BUFFER,b.Bytes.size(),b.Bytes.data(),GL_STREAM_DRAW);
        }
        for (const auto& a : draw.VertexAttributes) {
            const auto& b=*bindings[a.Binding];
            glBindBuffer(GL_ARRAY_BUFFER,vertices_[a.Binding]);
            glVertexAttribPointer(a.Location,a.ComponentCount,scalarTypes[static_cast<unsigned>(a.Format)],GL_FALSE,
                                  b.ByteStride,reinterpret_cast<void*>(static_cast<uintptr_t>(a.ByteOffset)));
            glEnableVertexAttribArray(a.Location);
        }
        const std::span<const uint8_t> payloads[]={draw.VertexUniformBytes,draw.FragmentUniformBytes};
        for (unsigned i=0;i<2;++i) {
            glBindBuffer(GL_UNIFORM_BUFFER,uniforms_[i]);
            glBufferData(GL_UNIFORM_BUFFER,std::max<size_t>(16,payloads[i].size()),
                         payloads[i].size()>=16?payloads[i].data():nullptr,GL_STREAM_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER,i==0?0:4,uniforms_[i]);
        }
        glUseProgram(program);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        for (const auto& texture : draw.Textures) {
            const GLuint unit=1+texture.Slot;
            glActiveTexture(GL_TEXTURE0+unit);
            glBindSampler(unit,0);
            glBindTexture(GL_TEXTURE_2D,textures_[texture.Slot]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,texture.Width,texture.Height,0,GL_RGBA,GL_UNSIGNED_BYTE,
                         decodedTextures[texture.Slot].data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,texture.MinLinear?GL_LINEAR:GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,texture.MagLinear?GL_LINEAR:GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,TextureWrap(texture.NativeWrapS));
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,TextureWrap(texture.NativeWrapT));
        }
        if (logic) glUniform1i(glGetUniformLocation(program,"mm3d_logic_operation"),static_cast<GLint>(draw.LogicOperation));
        glBindFramebuffer(GL_FRAMEBUFFER,target.Framebuffer());
        glDisable(GL_DITHER); glDisable(GL_RASTERIZER_DISCARD); glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_COVERAGE);
        glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        glColorMask(draw.ColorWriteMask&1,draw.ColorWriteMask&2,draw.ColorWriteMask&4,draw.ColorWriteMask&8);
        if (draw.Blend.Enabled) {
            glEnable(GL_BLEND); glBlendEquationSeparate(er,ea); glBlendFuncSeparate(sr,dr,sa,da);
            glBlendColor(draw.Blend.ConstantColor[0],draw.Blend.ConstantColor[1],draw.Blend.ConstantColor[2],draw.Blend.ConstantColor[3]);
        } else glDisable(GL_BLEND);
        if (draw.DepthTestEnabled || draw.DepthWriteEnabled) {
            glEnable(GL_DEPTH_TEST); glDepthFunc(draw.DepthTestEnabled?depth:GL_ALWAYS);
        } else glDisable(GL_DEPTH_TEST);
        glDepthMask(draw.DepthWriteEnabled); glDepthRangef(0,1);
        if (draw.Stencil.Enabled) {
            glEnable(GL_STENCIL_TEST); glStencilFunc(stencil,draw.Stencil.Reference,draw.Stencil.CompareMask);
            glStencilMask(draw.Stencil.WriteMask); glStencilOp(sf,sd,sp);
        } else glDisable(GL_STENCIL_TEST);
        if (draw.CullMode==NativeCullMode::KeepAll) glDisable(GL_CULL_FACE);
        else {
            glEnable(GL_CULL_FACE);
            glFrontFace(draw.CullMode==NativeCullMode::KeepCounterClockwise?GL_CCW:GL_CW);
            glCullFace(draw.FramebufferFlipped?GL_FRONT:GL_BACK);
        }
        glViewport(static_cast<GLint>(draw.ViewportX),static_cast<GLint>(draw.ViewportY),
                   static_cast<GLsizei>(draw.ViewportWidth),static_cast<GLsizei>(draw.ViewportHeight));
        glEnable(GL_SCISSOR_TEST);
        if (draw.ScissorMode==3)
            glScissor(draw.ScissorX1,draw.ScissorY1,draw.ScissorX2>=draw.ScissorX1?draw.ScissorX2-draw.ScissorX1+1:0,
                      draw.ScissorY2>=draw.ScissorY1?draw.ScissorY2-draw.ScissorY1+1:0);
        else glScissor(0,0,target.Width(),target.Height());
        if (draw.Indexed) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,draw.IndexBytes.size(),draw.IndexBytes.data(),GL_STREAM_DRAW);
        }
        Check(glGetError()==GL_NO_ERROR,"GLES error while preparing PICA draw");
        if (draw.Indexed) glDrawElementsBaseVertex(topology[static_cast<unsigned>(draw.Topology)],count,
                draw.IndicesAre16Bit?GL_UNSIGNED_SHORT:GL_UNSIGNED_BYTE,nullptr,draw.BaseVertex);
        else glDrawArrays(topology[static_cast<unsigned>(draw.Topology)],0,count);
        Check(glGetError()==GL_NO_ERROR,"GLES rejected the PICA draw");
        return true;
    } catch (const std::exception& failure) {
        if (error) *error=failure.what();
        return false;
    }
}
}

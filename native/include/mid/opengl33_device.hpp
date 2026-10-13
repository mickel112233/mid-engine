#pragma once
#include "mid/rendering.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace mid::render::gl33 {
using Enum=std::uint32_t; using UInt=std::uint32_t; using UInt64=std::uint64_t; using Int=std::int32_t; using Sizei=std::int32_t; using Sizeiptr=std::ptrdiff_t; using Boolean=std::uint8_t; using Float=float; using Char=char;
// Host supplies functions loaded by its platform loader (GLAD, SDL_GL_GetProcAddress, GLFW, etc.).
// A current OpenGL 3.3 core context is required for construction and every device call.
struct Functions {
 void (*GenBuffers)(Sizei,UInt*){}; void (*DeleteBuffers)(Sizei,const UInt*){}; void (*BindBuffer)(Enum,UInt){}; void (*BufferData)(Enum,Sizeiptr,const void*,Enum){};
 void (*GenVertexArrays)(Sizei,UInt*){}; void (*DeleteVertexArrays)(Sizei,const UInt*){}; void (*BindVertexArray)(UInt){};
 void (*EnableVertexAttribArray)(UInt){}; void (*DisableVertexAttribArray)(UInt){}; void (*VertexAttribPointer)(UInt,Int,Enum,Boolean,Sizei,const void*){}; void (*VertexAttribDivisor)(UInt,UInt){};
 UInt (*CreateShader)(Enum){}; void (*ShaderSource)(UInt,Sizei,const Char* const*,const Int*){}; void (*CompileShader)(UInt){}; void (*GetShaderiv)(UInt,Enum,Int*){}; void (*GetShaderInfoLog)(UInt,Sizei,Sizei*,Char*){}; void (*DeleteShader)(UInt){};
 UInt (*CreateProgram)(){}; void (*AttachShader)(UInt,UInt){}; void (*LinkProgram)(UInt){}; void (*GetProgramiv)(UInt,Enum,Int*){}; void (*GetProgramInfoLog)(UInt,Sizei,Sizei*,Char*){}; void (*DeleteProgram)(UInt){}; void (*UseProgram)(UInt){};
 Int (*GetUniformLocation)(UInt,const Char*){}; void (*UniformMatrix4fv)(Int,Sizei,Boolean,const Float*){}; void (*Uniform3f)(Int,Float,Float,Float){}; void (*Uniform4f)(Int,Float,Float,Float,Float){}; void (*Uniform1f)(Int,Float){}; void (*Uniform1i)(Int,Int){};
 void (*DrawElementsInstanced)(Enum,Sizei,Enum,const void*,Sizei){}; void (*DrawElements)(Enum,Sizei,Enum,const void*){};
 void (*Enable)(Enum){}; void (*Disable)(Enum){}; void (*BlendFunc)(Enum,Enum){}; void (*DepthMask)(Boolean){}; void (*DepthFunc)(Enum){}; void (*Clear)(Enum){}; void (*ClearColor)(Float,Float,Float,Float){}; void (*Viewport)(Int,Int,Sizei,Sizei){};
 void (*GenFramebuffers)(Sizei,UInt*){}; void (*DeleteFramebuffers)(Sizei,const UInt*){}; void (*BindFramebuffer)(Enum,UInt){}; void (*FramebufferTexture2D)(Enum,Enum,Enum,UInt,Int){}; void (*FramebufferRenderbuffer)(Enum,Enum,Enum,UInt){}; Enum (*CheckFramebufferStatus)(Enum){};
 void (*GenRenderbuffers)(Sizei,UInt*){}; void (*DeleteRenderbuffers)(Sizei,const UInt*){}; void (*BindRenderbuffer)(Enum,UInt){}; void (*RenderbufferStorage)(Enum,Enum,Sizei,Sizei){};
 void (*GenQueries)(Sizei,UInt*){}; void (*DeleteQueries)(Sizei,const UInt*){}; void (*BeginQuery)(Enum,UInt){}; void (*EndQuery)(Enum){}; void (*GetQueryObjectiv)(UInt,Enum,Int*){}; void (*GetQueryObjectui64v)(UInt,Enum,UInt64*){};
 void (*GenTextures)(Sizei,UInt*){}; void (*DeleteTextures)(Sizei,const UInt*){}; void (*BindTexture)(Enum,UInt){}; void (*TexImage2D)(Enum,Int,Int,Sizei,Sizei,Int,Enum,Enum,const void*){}; void (*TexParameteri)(Enum,Enum,Int){}; void (*ActiveTexture)(Enum){};
};
class OpenGL33Device final : public RenderDevice {
public:
 explicit OpenGL33Device(Functions functions);
 ~OpenGL33Device() override;
 OpenGL33Device(const OpenGL33Device&)=delete; OpenGL33Device& operator=(const OpenGL33Device&)=delete;
 [[nodiscard]] ResourceHandle createResource(ResourceKind kind,std::size_t bytes,const std::string& debugName) override;
 void destroyResource(ResourceHandle handle) noexcept override;
 void beginFrame() override;
 void submit(const RenderPlan& plan,const Camera& camera,const DirectionalLight& light) override;
 void endFrame() override;
 void registerMesh(ResourceId id,const MeshData& mesh);
 void registerMaterial(ResourceId id,const Material& material);
 void setEntityTransform(std::uint64_t entityId,const std::array<float,16>& columnMajorMatrix);
 void uploadRgba8(ResourceId texture,std::uint32_t width,std::uint32_t height,const std::uint8_t* pixels,std::size_t byteCount);
 void bindBaseColorTexture(ResourceId material,ResourceId texture);
 void setViewport(Sizei width,Sizei height);
 [[nodiscard]] const RenderStats& lastStats() const noexcept;
private:
 struct GpuMesh { UInt vao{0},vertexBuffer{0},indexBuffer{0},instanceBuffer{0}; Sizei indexCount{0}; };
 struct GpuMaterial { Material value; ResourceId texture{0}; };
 struct GpuResource { ResourceKind kind{ResourceKind::Other}; UInt handle{0}; };
 [[nodiscard]] UInt compile(Enum type,const char* source);
 [[nodiscard]] Int uniform(const char* name) const;
 Functions gl_; UInt program_{0},shadowProgram_{0},shadowFramebuffer_{0},shadowTexture_{0},shadowColorBuffer_{0},gpuQuery_{0}; ResourceId nextResource_{1};
 Sizei viewportWidth_{1280},viewportHeight_{720}; Sizei shadowSize_{1024}; bool shadowEnabled_{false},queryPending_{false},queryActive_{false}; double lastGpuMilliseconds_{0.0}; bool gpuTimingAvailable_{false};
 std::unordered_map<ResourceId,GpuMesh> meshes_;
 std::unordered_map<ResourceId,GpuMaterial> materials_;
 std::unordered_map<ResourceId,GpuResource> resources_;
 std::unordered_map<std::uint64_t,std::array<float,16>> transforms_;
 RenderStats stats_{};
};
}

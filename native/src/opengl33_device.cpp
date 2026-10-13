#include "mid/opengl33_device.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace mid::render::gl33 {
namespace {
constexpr Enum GL_ARRAY_BUFFER=0x8892,GL_ELEMENT_ARRAY_BUFFER=0x8893,GL_STATIC_DRAW=0x88E4,GL_DYNAMIC_DRAW=0x88E8;
constexpr Enum GL_FLOAT=0x1406,GL_UNSIGNED_INT=0x1405,GL_TRIANGLES=0x0004,GL_TEXTURE_2D=0x0DE1,GL_RGBA=0x1908,GL_RGBA8=0x8058,GL_UNSIGNED_BYTE=0x1401;
constexpr Enum GL_TEXTURE_MIN_FILTER=0x2801,GL_TEXTURE_MAG_FILTER=0x2800,GL_TEXTURE_WRAP_S=0x2802,GL_TEXTURE_WRAP_T=0x2803,GL_LINEAR=0x2601,GL_NEAREST=0x2600,GL_REPEAT=0x2901,GL_CLAMP_TO_EDGE=0x812F;
constexpr Enum GL_VERTEX_SHADER=0x8B31,GL_FRAGMENT_SHADER=0x8B30,GL_COMPILE_STATUS=0x8B81,GL_LINK_STATUS=0x8B82,GL_INFO_LOG_LENGTH=0x8B84;
constexpr Enum GL_DEPTH_TEST=0x0B71,GL_LEQUAL=0x0203,GL_COLOR_BUFFER_BIT=0x00004000,GL_DEPTH_BUFFER_BIT=0x00000100,GL_TEXTURE0=0x84C0,GL_TEXTURE1=0x84C1,GL_BLEND=0x0BE2,GL_SRC_ALPHA=0x0302,GL_ONE_MINUS_SRC_ALPHA=0x0303;
constexpr Enum GL_FRAMEBUFFER=0x8D40,GL_DEPTH_ATTACHMENT=0x8D00,GL_COLOR_ATTACHMENT0=0x8CE0,GL_RENDERBUFFER=0x8D41,GL_RGBA4=0x8056,GL_DEPTH_COMPONENT=0x1902,GL_FRAMEBUFFER_COMPLETE=0x8CD5,GL_TIME_ELAPSED=0x88BF,GL_QUERY_RESULT_AVAILABLE=0x8867,GL_QUERY_RESULT=0x8866;
struct V3 {float x,y,z;};
V3 mul(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 norm(V3 a){const float n=std::sqrt(dot(a,a));if(!std::isfinite(n)||n<1e-7F)throw std::invalid_argument("Camera basis is invalid");return mul(a,1.0F/n);}
std::array<float,16> multiply(const std::array<float,16>& a,const std::array<float,16>& b){std::array<float,16> o{};for(int c=0;c<4;++c)for(int r=0;r<4;++r)o[static_cast<std::size_t>(c*4+r)]=a[static_cast<std::size_t>(r)]*b[static_cast<std::size_t>(c*4)]+a[static_cast<std::size_t>(4+r)]*b[static_cast<std::size_t>(c*4+1)]+a[static_cast<std::size_t>(8+r)]*b[static_cast<std::size_t>(c*4+2)]+a[static_cast<std::size_t>(12+r)]*b[static_cast<std::size_t>(c*4+3)];return o;}
std::array<float,16> viewProjection(const Camera& camera){
 const V3 p{camera.position.x,camera.position.y,camera.position.z},f=norm({camera.forward.x,camera.forward.y,camera.forward.z}),up{camera.up.x,camera.up.y,camera.up.z};
 const V3 s=norm(cross(f,up)),u=cross(s,f);
 std::array<float,16> view={s.x,u.x,-f.x,0,s.y,u.y,-f.y,0,s.z,u.z,-f.z,0,-dot(s,p),-dot(u,p),dot(f,p),1};
 const float t=1.0F/std::tan(camera.verticalFovRadians*0.5F),nf=1.0F/(camera.nearPlane-camera.farPlane);
 std::array<float,16> projection{};projection[0]=t/camera.aspectRatio;projection[5]=t;projection[10]=(camera.farPlane+camera.nearPlane)*nf;projection[11]=-1;projection[14]=2*camera.farPlane*camera.nearPlane*nf;
 return multiply(projection,view);
}
std::array<float,16> lightViewProjection(const DirectionalLight& light){
 const V3 direction=norm({light.direction.x,light.direction.y,light.direction.z});
 const V3 eye=mul(direction,-18.0F),up=std::abs(direction.y)>0.95F?V3{0,0,1}:V3{0,1,0};
 const V3 s=norm(cross(direction,up)),u=cross(s,direction);
 const std::array<float,16> view={s.x,u.x,-direction.x,0,s.y,u.y,-direction.y,0,s.z,u.z,-direction.z,0,-dot(s,eye),-dot(u,eye),dot(direction,eye),1};
 constexpr float nearPlane=0.1F,farPlane=70.0F;
 std::array<float,16> projection{};projection[0]=1.0F/18.0F;projection[5]=1.0F/18.0F;projection[10]=-2.0F/(farPlane-nearPlane);projection[14]=-(farPlane+nearPlane)/(farPlane-nearPlane);projection[15]=1.0F;
 return multiply(projection,view);
}
constexpr const char* VERTEX_SHADER=R"(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
layout(location=3) in vec4 aColor;
layout(location=4) in mat4 iModel;
uniform mat4 uViewProjection;
uniform mat4 uLightViewProjection;
uniform mat4 uModel;
uniform int uInstanced;
out vec3 vWorldPosition; out vec3 vNormal; out vec2 vUv; out vec4 vColor; out vec4 vShadowPosition;
void main(){mat4 model=uInstanced==1?iModel:uModel;vec4 world=model*vec4(aPosition,1.0);vWorldPosition=world.xyz;vShadowPosition=uLightViewProjection*world;vNormal=normalize(mat3(model)*aNormal);vUv=aUv;vColor=aColor;gl_Position=uViewProjection*world;})";
constexpr const char* FRAGMENT_SHADER=R"(#version 330 core
in vec3 vWorldPosition; in vec3 vNormal; in vec2 vUv; in vec4 vColor; in vec4 vShadowPosition;
uniform sampler2D uShadowMap; uniform int uShadowEnabled;
uniform vec4 uBaseColor; uniform vec3 uLightDirection; uniform vec3 uLightColor; uniform float uLightIntensity; uniform vec3 uCameraPosition; uniform float uMetallic; uniform float uRoughness; uniform int uHasTexture; uniform sampler2D uBaseColorTexture;
out vec4 outColor;
void main(){vec4 base=uBaseColor*vColor;if(uHasTexture==1)base*=texture(uBaseColorTexture,vUv);vec3 N=normalize(vNormal),V=normalize(uCameraPosition-vWorldPosition),L=normalize(-uLightDirection),H=normalize(V+L);float nl=max(dot(N,L),0.0),nv=max(dot(N,V),0.001),nh=max(dot(N,H),0.0),vh=max(dot(V,H),0.0);float r=clamp(uRoughness,0.05,1.0),a=r*r,a2=a*a,d=nh*nh*(a2-1.0)+1.0;float D=a2/max(3.14159265*d*d,0.001),k=(r+1.0)*(r+1.0)/8.0,G=(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));vec3 f0=mix(vec3(0.04),base.rgb,uMetallic),F=f0+(1.0-f0)*pow(1.0-vh,5.0);vec3 spec=D*G*F/max(4.0*nv*max(nl,0.001),0.001),diff=(1.0-F)*(1.0-uMetallic)*base.rgb/3.14159265;float visibility=1.0;if(uShadowEnabled==1&&vShadowPosition.w>0.0){vec3 sc=vShadowPosition.xyz/vShadowPosition.w*0.5+0.5;if(sc.x>0.0&&sc.x<1.0&&sc.y>0.0&&sc.y<1.0&&sc.z>0.0&&sc.z<1.0){float depth=texture(uShadowMap,sc.xy).r;visibility=sc.z-0.0025<=depth?1.0:0.28;}}vec3 hemi=mix(vec3(0.07,0.06,0.05),vec3(0.25,0.34,0.48),clamp(N.y*0.5+0.5,0.0,1.0));outColor=vec4(base.rgb*hemi*0.35+uLightColor*uLightIntensity*(diff+spec)*nl*visibility,base.a);})";
constexpr const char* SHADOW_VERTEX_SHADER=R"(#version 330 core
layout(location=0) in vec3 aPosition;
uniform mat4 uModel; uniform mat4 uLightViewProjection;
void main(){gl_Position=uLightViewProjection*uModel*vec4(aPosition,1.0);})";
constexpr const char* SHADOW_FRAGMENT_SHADER=R"(#version 330 core
out vec4 outColor; void main(){outColor=vec4(1.0);})";
bool allShadowFunctions(const Functions& g){return g.GenFramebuffers&&g.DeleteFramebuffers&&g.BindFramebuffer&&g.FramebufferTexture2D&&g.FramebufferRenderbuffer&&g.CheckFramebufferStatus&&g.GenRenderbuffers&&g.DeleteRenderbuffers&&g.BindRenderbuffer&&g.RenderbufferStorage;}
bool allQueryFunctions(const Functions& g){return g.GenQueries&&g.DeleteQueries&&g.BeginQuery&&g.EndQuery&&g.GetQueryObjectiv&&g.GetQueryObjectui64v;}
bool allFunctions(const Functions& g){
 return g.GenBuffers&&g.DeleteBuffers&&g.BindBuffer&&g.BufferData&&g.GenVertexArrays&&g.DeleteVertexArrays&&g.BindVertexArray&&g.EnableVertexAttribArray&&g.DisableVertexAttribArray&&g.VertexAttribPointer&&g.VertexAttribDivisor&&g.CreateShader&&g.ShaderSource&&g.CompileShader&&g.GetShaderiv&&g.GetShaderInfoLog&&g.DeleteShader&&g.CreateProgram&&g.AttachShader&&g.LinkProgram&&g.GetProgramiv&&g.GetProgramInfoLog&&g.DeleteProgram&&g.UseProgram&&g.GetUniformLocation&&g.UniformMatrix4fv&&g.Uniform3f&&g.Uniform4f&&g.Uniform1f&&g.Uniform1i&&g.DrawElementsInstanced&&g.DrawElements&&g.Enable&&g.Disable&&g.BlendFunc&&g.DepthMask&&g.DepthFunc&&g.Clear&&g.ClearColor&&g.Viewport&&g.GenTextures&&g.DeleteTextures&&g.BindTexture&&g.TexImage2D&&g.TexParameteri&&g.ActiveTexture;
}
}
OpenGL33Device::OpenGL33Device(Functions functions):gl_(functions){
 if(!allFunctions(gl_))throw std::invalid_argument("OpenGL 3.3 function table is incomplete");
 const UInt vertex=compile(GL_VERTEX_SHADER,VERTEX_SHADER);
 UInt fragment=0;
 try{fragment=compile(GL_FRAGMENT_SHADER,FRAGMENT_SHADER);program_=gl_.CreateProgram();if(!program_)throw std::runtime_error("OpenGL could not create a shader program");gl_.AttachShader(program_,vertex);gl_.AttachShader(program_,fragment);gl_.LinkProgram(program_);Int ok=0;gl_.GetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok){Int n=0;gl_.GetProgramiv(program_,GL_INFO_LOG_LENGTH,&n);std::string log(static_cast<std::size_t>(std::max(1,n)),'\0');gl_.GetProgramInfoLog(program_,static_cast<Sizei>(log.size()),nullptr,log.data());throw std::runtime_error("OpenGL PBR program link failed: "+log);}}
 catch(...){if(fragment)gl_.DeleteShader(fragment);gl_.DeleteShader(vertex);if(program_)gl_.DeleteProgram(program_);program_=0;throw;}
 gl_.DeleteShader(vertex);gl_.DeleteShader(fragment);
 if(allShadowFunctions(gl_)){
  try{
   const UInt sv=compile(GL_VERTEX_SHADER,SHADOW_VERTEX_SHADER),sf=compile(GL_FRAGMENT_SHADER,SHADOW_FRAGMENT_SHADER);
   shadowProgram_=gl_.CreateProgram();gl_.AttachShader(shadowProgram_,sv);gl_.AttachShader(shadowProgram_,sf);gl_.LinkProgram(shadowProgram_);
   Int linked=0;gl_.GetProgramiv(shadowProgram_,GL_LINK_STATUS,&linked);gl_.DeleteShader(sv);gl_.DeleteShader(sf);
   if(!linked)throw std::runtime_error("Native shadow shader failed to link");
   gl_.GenTextures(1,&shadowTexture_);gl_.BindTexture(GL_TEXTURE_2D,shadowTexture_);
   gl_.TexImage2D(GL_TEXTURE_2D,0,static_cast<Int>(GL_DEPTH_COMPONENT),shadowSize_,shadowSize_,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
   gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,static_cast<Int>(GL_NEAREST));gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,static_cast<Int>(GL_NEAREST));
   gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,static_cast<Int>(GL_CLAMP_TO_EDGE));gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,static_cast<Int>(GL_CLAMP_TO_EDGE));
   gl_.GenRenderbuffers(1,&shadowColorBuffer_);gl_.BindRenderbuffer(GL_RENDERBUFFER,shadowColorBuffer_);gl_.RenderbufferStorage(GL_RENDERBUFFER,GL_RGBA4,shadowSize_,shadowSize_);
   gl_.GenFramebuffers(1,&shadowFramebuffer_);gl_.BindFramebuffer(GL_FRAMEBUFFER,shadowFramebuffer_);
   gl_.FramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,shadowTexture_,0);gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,shadowColorBuffer_);
   shadowEnabled_=gl_.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;gl_.BindFramebuffer(GL_FRAMEBUFFER,0);
   if(!shadowEnabled_)throw std::runtime_error("Native shadow framebuffer is incomplete");
  }catch(...){
   if(shadowFramebuffer_)gl_.DeleteFramebuffers(1,&shadowFramebuffer_);if(shadowTexture_)gl_.DeleteTextures(1,&shadowTexture_);if(shadowColorBuffer_)gl_.DeleteRenderbuffers(1,&shadowColorBuffer_);if(shadowProgram_)gl_.DeleteProgram(shadowProgram_);
   shadowFramebuffer_=shadowTexture_=shadowColorBuffer_=shadowProgram_=0;shadowEnabled_=false;gl_.BindFramebuffer(GL_FRAMEBUFFER,0);
  }
 }
 gl_.Enable(GL_DEPTH_TEST);gl_.DepthFunc(GL_LEQUAL);gl_.ClearColor(0.075F,0.083F,0.105F,1.0F);
}
OpenGL33Device::~OpenGL33Device(){
 for(auto& [id,m]:meshes_){(void)id;if(m.vertexBuffer)gl_.DeleteBuffers(1,&m.vertexBuffer);if(m.indexBuffer)gl_.DeleteBuffers(1,&m.indexBuffer);if(m.instanceBuffer)gl_.DeleteBuffers(1,&m.instanceBuffer);if(m.vao)gl_.DeleteVertexArrays(1,&m.vao);}
 for(auto& [id,r]:resources_){(void)id;if(r.kind==ResourceKind::Buffer)gl_.DeleteBuffers(1,&r.handle);else if(r.kind==ResourceKind::Texture)gl_.DeleteTextures(1,&r.handle);}
 if(gpuQuery_&&allQueryFunctions(gl_))gl_.DeleteQueries(1,&gpuQuery_);
 if(shadowFramebuffer_&&gl_.DeleteFramebuffers)gl_.DeleteFramebuffers(1,&shadowFramebuffer_);
 if(shadowTexture_)gl_.DeleteTextures(1,&shadowTexture_);
 if(shadowColorBuffer_&&gl_.DeleteRenderbuffers)gl_.DeleteRenderbuffers(1,&shadowColorBuffer_);
 if(shadowProgram_)gl_.DeleteProgram(shadowProgram_);
 if(program_)gl_.DeleteProgram(program_);
}
UInt OpenGL33Device::compile(Enum type,const char* source){
 const UInt shader=gl_.CreateShader(type);if(!shader)throw std::runtime_error("OpenGL could not create shader");
 gl_.ShaderSource(shader,1,&source,nullptr);gl_.CompileShader(shader);Int ok=0;gl_.GetShaderiv(shader,GL_COMPILE_STATUS,&ok);
 if(!ok){Int n=0;gl_.GetShaderiv(shader,GL_INFO_LOG_LENGTH,&n);std::string log(static_cast<std::size_t>(std::max(1,n)),'\0');gl_.GetShaderInfoLog(shader,static_cast<Sizei>(log.size()),nullptr,log.data());gl_.DeleteShader(shader);throw std::runtime_error("OpenGL shader compilation failed: "+log);}return shader;
}
Int OpenGL33Device::uniform(const char* name)const{return gl_.GetUniformLocation(program_,name);}
ResourceHandle OpenGL33Device::createResource(ResourceKind kind,std::size_t bytes,const std::string&){
 if(nextResource_==0||nextResource_==std::numeric_limits<ResourceId>::max())throw std::overflow_error("OpenGL resource ID space exhausted");
 UInt handle=0;
 if(kind==ResourceKind::Buffer){if(bytes>static_cast<std::size_t>(std::numeric_limits<Sizeiptr>::max()))throw std::length_error("GPU buffer is too large");gl_.GenBuffers(1,&handle);gl_.BindBuffer(GL_ARRAY_BUFFER,handle);gl_.BufferData(GL_ARRAY_BUFFER,static_cast<Sizeiptr>(bytes),nullptr,GL_DYNAMIC_DRAW);}
 else if(kind==ResourceKind::Texture){gl_.GenTextures(1,&handle);gl_.BindTexture(GL_TEXTURE_2D,handle);gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);gl_.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);}
 else throw std::invalid_argument("OpenGL33Device creates Buffer and Texture resources directly; shaders/pipelines are device-owned");
 if(!handle)throw std::runtime_error("OpenGL failed to allocate a GPU resource");
 const auto id=nextResource_++;resources_.emplace(id,GpuResource{kind,handle});return {id,kind};
}
void OpenGL33Device::destroyResource(ResourceHandle h)noexcept{
 const auto it=resources_.find(h.id);if(it==resources_.end()||it->second.kind!=h.kind)return;
 if(h.kind==ResourceKind::Buffer)gl_.DeleteBuffers(1,&it->second.handle);else if(h.kind==ResourceKind::Texture)gl_.DeleteTextures(1,&it->second.handle);
 resources_.erase(it);
}
void OpenGL33Device::beginFrame(){
 stats_={};
 if(allQueryFunctions(gl_)){
  if(queryPending_){Int available=0;gl_.GetQueryObjectiv(gpuQuery_,GL_QUERY_RESULT_AVAILABLE,&available);if(available){UInt64 nanoseconds=0;gl_.GetQueryObjectui64v(gpuQuery_,GL_QUERY_RESULT,&nanoseconds);lastGpuMilliseconds_=static_cast<double>(nanoseconds)/1000000.0;gpuTimingAvailable_=true;queryPending_=false;}}
  if(!queryPending_&&!queryActive_){if(!gpuQuery_)gl_.GenQueries(1,&gpuQuery_);if(gpuQuery_){gl_.BeginQuery(GL_TIME_ELAPSED,gpuQuery_);queryActive_=true;}}
 }
 gl_.BindFramebuffer(GL_FRAMEBUFFER,0);gl_.Viewport(0,0,viewportWidth_,viewportHeight_);gl_.Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
void OpenGL33Device::registerMesh(ResourceId id,const MeshData& input){
 if(id==0)throw std::invalid_argument("Mesh resource ID cannot be zero");
 input.validate();
 if(input.vertices.size()>static_cast<std::size_t>(std::numeric_limits<Sizeiptr>::max())/sizeof(Vertex))throw std::length_error("Mesh vertex data is too large");
 std::vector<std::uint32_t> generated;const auto* indices=&input.indices;if(indices->empty()){generated.resize(input.vertices.size());for(std::size_t i=0;i<generated.size();++i)generated[i]=static_cast<std::uint32_t>(i);indices=&generated;}
 if(indices->size()>static_cast<std::size_t>(std::numeric_limits<Sizei>::max()))throw std::length_error("Mesh index count is too large");
 if(auto it=meshes_.find(id);it!=meshes_.end()){auto old=it->second;if(old.vertexBuffer)gl_.DeleteBuffers(1,&old.vertexBuffer);if(old.indexBuffer)gl_.DeleteBuffers(1,&old.indexBuffer);if(old.instanceBuffer)gl_.DeleteBuffers(1,&old.instanceBuffer);if(old.vao)gl_.DeleteVertexArrays(1,&old.vao);meshes_.erase(it);}
 GpuMesh gpu;gpu.indexCount=static_cast<Sizei>(indices->size());gl_.GenVertexArrays(1,&gpu.vao);gl_.GenBuffers(1,&gpu.vertexBuffer);gl_.GenBuffers(1,&gpu.indexBuffer);gl_.GenBuffers(1,&gpu.instanceBuffer);
 gl_.BindVertexArray(gpu.vao);gl_.BindBuffer(GL_ARRAY_BUFFER,gpu.vertexBuffer);gl_.BufferData(GL_ARRAY_BUFFER,static_cast<Sizeiptr>(input.vertices.size()*sizeof(Vertex)),input.vertices.data(),GL_STATIC_DRAW);
 gl_.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.indexBuffer);gl_.BufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<Sizeiptr>(indices->size()*sizeof(std::uint32_t)),indices->data(),GL_STATIC_DRAW);
 gl_.EnableVertexAttribArray(0);gl_.VertexAttribPointer(0,3,GL_FLOAT,0,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,position)));
 gl_.EnableVertexAttribArray(1);gl_.VertexAttribPointer(1,3,GL_FLOAT,0,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,normal)));
 gl_.EnableVertexAttribArray(2);gl_.VertexAttribPointer(2,2,GL_FLOAT,0,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,uv)));
 gl_.EnableVertexAttribArray(3);gl_.VertexAttribPointer(3,4,GL_FLOAT,0,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,color)));
 gl_.BindVertexArray(0);meshes_.emplace(id,gpu);
}
void OpenGL33Device::registerMaterial(ResourceId id,const Material& material){if(id==0)throw std::invalid_argument("Material resource ID cannot be zero");material.validate();materials_[id]={material,0};}
void OpenGL33Device::setEntityTransform(std::uint64_t id,const std::array<float,16>& matrix){if(id==0)throw std::invalid_argument("Entity ID cannot be zero");for(float v:matrix)if(!std::isfinite(v))throw std::invalid_argument("Entity transform matrix must be finite");transforms_[id]=matrix;}
void OpenGL33Device::uploadRgba8(ResourceId id,std::uint32_t width,std::uint32_t height,const std::uint8_t* pixels,std::size_t byteCount){
 const auto it=resources_.find(id);if(it==resources_.end()||it->second.kind!=ResourceKind::Texture)throw std::out_of_range("Texture resource does not exist");
 if(width==0||height==0||pixels==nullptr||static_cast<std::uint64_t>(width)*height>std::numeric_limits<std::size_t>::max()/4U||byteCount!=static_cast<std::size_t>(width)*height*4U)throw std::invalid_argument("RGBA8 texture dimensions/data are invalid");
 if(width>static_cast<std::uint32_t>(std::numeric_limits<Sizei>::max())||height>static_cast<std::uint32_t>(std::numeric_limits<Sizei>::max()))throw std::length_error("Texture dimensions are too large");
 gl_.BindTexture(GL_TEXTURE_2D,it->second.handle);gl_.TexImage2D(GL_TEXTURE_2D,0,static_cast<Int>(GL_RGBA8),static_cast<Sizei>(width),static_cast<Sizei>(height),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
}
void OpenGL33Device::bindBaseColorTexture(ResourceId material,ResourceId texture){
 auto m=materials_.find(material);auto t=resources_.find(texture);if(m==materials_.end())throw std::out_of_range("Material resource does not exist");if(t==resources_.end()||t->second.kind!=ResourceKind::Texture)throw std::out_of_range("Base-color texture resource does not exist");m->second.texture=texture;
}
void OpenGL33Device::submit(const RenderPlan& plan,const Camera& camera,const DirectionalLight& light){
 camera.validate();const auto vp=viewProjection(camera);const auto lightVp=lightViewProjection(light);std::size_t shadowDrawCalls=0;
 if(shadowEnabled_&&shadowFramebuffer_&&shadowProgram_){
  gl_.BindFramebuffer(GL_FRAMEBUFFER,shadowFramebuffer_);gl_.Viewport(0,0,shadowSize_,shadowSize_);gl_.Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);gl_.UseProgram(shadowProgram_);
  const Int shadowModel=gl_.GetUniformLocation(shadowProgram_,"uModel"),shadowVp=gl_.GetUniformLocation(shadowProgram_,"uLightViewProjection");gl_.UniformMatrix4fv(shadowVp,1,0,lightVp.data());
  auto castShadows=[&](const std::vector<DrawBatch>& batches){for(const auto& batch:batches){const auto mesh=meshes_.find(batch.mesh);if(mesh==meshes_.end())continue;gl_.BindVertexArray(mesh->second.vao);for(const auto entity:batch.entityIds){const auto tr=transforms_.find(entity);if(tr==transforms_.end())continue;gl_.UniformMatrix4fv(shadowModel,1,0,tr->second.data());gl_.DrawElements(GL_TRIANGLES,mesh->second.indexCount,GL_UNSIGNED_INT,nullptr);++shadowDrawCalls;}}};
  castShadows(plan.opaqueBatches);castShadows(plan.transparentBatches);gl_.BindVertexArray(0);gl_.BindFramebuffer(GL_FRAMEBUFFER,0);gl_.Viewport(0,0,viewportWidth_,viewportHeight_);
 }
 gl_.UseProgram(program_);gl_.UniformMatrix4fv(uniform("uViewProjection"),1,0,vp.data());gl_.UniformMatrix4fv(uniform("uLightViewProjection"),1,0,lightVp.data());gl_.Uniform1i(uniform("uShadowEnabled"),shadowEnabled_?1:0);gl_.Uniform3f(uniform("uLightDirection"),light.direction.x,light.direction.y,light.direction.z);gl_.Uniform3f(uniform("uLightColor"),light.color.x,light.color.y,light.color.z);gl_.Uniform1f(uniform("uLightIntensity"),light.intensity);gl_.Uniform3f(uniform("uCameraPosition"),camera.position.x,camera.position.y,camera.position.z);gl_.Uniform1i(uniform("uBaseColorTexture"),0);
 if(shadowEnabled_&&shadowTexture_){gl_.ActiveTexture(GL_TEXTURE1);gl_.BindTexture(GL_TEXTURE_2D,shadowTexture_);gl_.Uniform1i(uniform("uShadowMap"),1);gl_.ActiveTexture(GL_TEXTURE0);}
 stats_=plan.stats;stats_.drawCalls=shadowDrawCalls;stats_.batches=0;stats_.estimatedInstanceCount=0;
 auto drawBatches=[&](const std::vector<DrawBatch>& batches,bool transparent){
  for(const auto& batch:batches){const auto meshIt=meshes_.find(batch.mesh);const auto materialIt=materials_.find(batch.material);if(meshIt==meshes_.end()||materialIt==materials_.end())continue;const auto& mesh=meshIt->second;const auto& mat=materialIt->second;
   gl_.Uniform4f(uniform("uBaseColor"),mat.value.baseColor.x,mat.value.baseColor.y,mat.value.baseColor.z,mat.value.baseColor.w);gl_.Uniform1f(uniform("uMetallic"),mat.value.metallic);gl_.Uniform1f(uniform("uRoughness"),mat.value.roughness);
   const auto texture=resources_.find(mat.texture);const bool hasTexture=mat.texture!=0&&texture!=resources_.end()&&texture->second.kind==ResourceKind::Texture;gl_.Uniform1i(uniform("uHasTexture"),hasTexture?1:0);if(hasTexture){gl_.ActiveTexture(GL_TEXTURE0);gl_.BindTexture(GL_TEXTURE_2D,texture->second.handle);}
   std::vector<std::array<float,16>> instances;instances.reserve(batch.entityIds.size());for(auto entity:batch.entityIds){const auto tr=transforms_.find(entity);if(tr!=transforms_.end())instances.push_back(tr->second);}
   if(transparent)std::sort(instances.begin(),instances.end(),[&](const auto& a,const auto& b){const float ax=a[12]-camera.position.x,ay=a[13]-camera.position.y,az=a[14]-camera.position.z,bx=b[12]-camera.position.x,by=b[13]-camera.position.y,bz=b[14]-camera.position.z;return ax*ax+ay*ay+az*az>bx*bx+by*by+bz*bz;});
   if(instances.empty())continue;
   gl_.BindVertexArray(mesh.vao);
   if(instances.size()>1){std::vector<float> data;data.reserve(instances.size()*16);for(const auto& m:instances)data.insert(data.end(),m.begin(),m.end());gl_.BindBuffer(GL_ARRAY_BUFFER,mesh.instanceBuffer);gl_.BufferData(GL_ARRAY_BUFFER,static_cast<Sizeiptr>(data.size()*sizeof(float)),data.data(),GL_DYNAMIC_DRAW);
    for(UInt column=0;column<4;++column){const UInt loc=4+column;gl_.EnableVertexAttribArray(loc);gl_.VertexAttribPointer(loc,4,GL_FLOAT,0,16*static_cast<Sizei>(sizeof(float)),reinterpret_cast<const void*>(static_cast<std::uintptr_t>(column*4*sizeof(float))));gl_.VertexAttribDivisor(loc,1);}
    gl_.Uniform1i(uniform("uInstanced"),1);gl_.DrawElementsInstanced(GL_TRIANGLES,mesh.indexCount,GL_UNSIGNED_INT,nullptr,static_cast<Sizei>(instances.size()));
    for(UInt column=0;column<4;++column){const UInt loc=4+column;gl_.VertexAttribDivisor(loc,0);gl_.DisableVertexAttribArray(loc);}gl_.Uniform1i(uniform("uInstanced"),0);++stats_.drawCalls;stats_.estimatedInstanceCount+=instances.size();
   }else{gl_.UniformMatrix4fv(uniform("uModel"),1,0,instances.front().data());gl_.Uniform1i(uniform("uInstanced"),0);gl_.DrawElements(GL_TRIANGLES,mesh.indexCount,GL_UNSIGNED_INT,nullptr);++stats_.drawCalls;++stats_.estimatedInstanceCount;}
   ++stats_.batches;
  }
 };
 drawBatches(plan.opaqueBatches,false);gl_.Enable(GL_BLEND);gl_.BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);gl_.DepthMask(0);drawBatches(plan.transparentBatches,true);gl_.DepthMask(1);gl_.Disable(GL_BLEND);gl_.BindVertexArray(0);
}
void OpenGL33Device::endFrame(){if(queryActive_){gl_.EndQuery(GL_TIME_ELAPSED);queryActive_=false;queryPending_=true;}stats_.gpuMilliseconds=lastGpuMilliseconds_;stats_.gpuTimingAvailable=gpuTimingAvailable_;}
void OpenGL33Device::setViewport(Sizei width,Sizei height){if(width<=0||height<=0)throw std::invalid_argument("Viewport dimensions must be positive");viewportWidth_=width;viewportHeight_=height;gl_.Viewport(0,0,width,height);}
const RenderStats& OpenGL33Device::lastStats()const noexcept{return stats_;}
}

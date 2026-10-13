#include "mid/native_desktop_host.hpp"
#ifdef MID_ENGINE_WITH_SDL2
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace mid::platform {
namespace {
using namespace render::gl33;
Functions loadOpenGL(){
 Functions g{};
#define MID_LOAD_GL(name) g.name=reinterpret_cast<decltype(g.name)>(SDL_GL_GetProcAddress("gl" #name))
 MID_LOAD_GL(GenBuffers);MID_LOAD_GL(DeleteBuffers);MID_LOAD_GL(BindBuffer);MID_LOAD_GL(BufferData);
 MID_LOAD_GL(GenVertexArrays);MID_LOAD_GL(DeleteVertexArrays);MID_LOAD_GL(BindVertexArray);MID_LOAD_GL(EnableVertexAttribArray);MID_LOAD_GL(DisableVertexAttribArray);MID_LOAD_GL(VertexAttribPointer);MID_LOAD_GL(VertexAttribDivisor);
 MID_LOAD_GL(CreateShader);MID_LOAD_GL(ShaderSource);MID_LOAD_GL(CompileShader);MID_LOAD_GL(GetShaderiv);MID_LOAD_GL(GetShaderInfoLog);MID_LOAD_GL(DeleteShader);
 MID_LOAD_GL(CreateProgram);MID_LOAD_GL(AttachShader);MID_LOAD_GL(LinkProgram);MID_LOAD_GL(GetProgramiv);MID_LOAD_GL(GetProgramInfoLog);MID_LOAD_GL(DeleteProgram);MID_LOAD_GL(UseProgram);
 MID_LOAD_GL(GetUniformLocation);MID_LOAD_GL(UniformMatrix4fv);MID_LOAD_GL(Uniform3f);MID_LOAD_GL(Uniform4f);MID_LOAD_GL(Uniform1f);MID_LOAD_GL(Uniform1i);
 MID_LOAD_GL(DrawElementsInstanced);MID_LOAD_GL(DrawElements);MID_LOAD_GL(Enable);MID_LOAD_GL(Disable);MID_LOAD_GL(BlendFunc);MID_LOAD_GL(DepthMask);MID_LOAD_GL(DepthFunc);MID_LOAD_GL(Clear);MID_LOAD_GL(ClearColor);MID_LOAD_GL(Viewport);
 MID_LOAD_GL(GenFramebuffers);MID_LOAD_GL(DeleteFramebuffers);MID_LOAD_GL(BindFramebuffer);MID_LOAD_GL(FramebufferTexture2D);MID_LOAD_GL(FramebufferRenderbuffer);MID_LOAD_GL(CheckFramebufferStatus);
 MID_LOAD_GL(GenRenderbuffers);MID_LOAD_GL(DeleteRenderbuffers);MID_LOAD_GL(BindRenderbuffer);MID_LOAD_GL(RenderbufferStorage);
 MID_LOAD_GL(GenQueries);MID_LOAD_GL(DeleteQueries);MID_LOAD_GL(BeginQuery);MID_LOAD_GL(EndQuery);MID_LOAD_GL(GetQueryObjectiv);MID_LOAD_GL(GetQueryObjectui64v);
 MID_LOAD_GL(GenTextures);MID_LOAD_GL(DeleteTextures);MID_LOAD_GL(BindTexture);MID_LOAD_GL(TexImage2D);MID_LOAD_GL(TexParameteri);MID_LOAD_GL(ActiveTexture);
#undef MID_LOAD_GL
 return g;
}
render::Vec3f add(render::Vec3f a,render::Vec3f b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
render::Vec3f mul(render::Vec3f a,float s){return {a.x*s,a.y*s,a.z*s};}
float dot(render::Vec3f a,render::Vec3f b){return a.x*b.x+a.y*b.y+a.z*b.z;}
render::Vec3f normalized(render::Vec3f a){const float n=std::sqrt(dot(a,a));return n>1e-6F?mul(a,1.0F/n):render::Vec3f{0,0,-1};}
render::Vec3f cross(render::Vec3f a,render::Vec3f b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
}
struct NativeDesktopHost::Impl {
 editor::EditorSession* session;editor::SceneRenderBridge* bridge;
 std::unordered_map<render::ResourceId,render::Material>* materials;
 SDL_Window* window{nullptr};SDL_GLContext context{nullptr};
 std::unique_ptr<render::gl33::OpenGL33Device> device;
 std::unique_ptr<editor::NativeEditorRuntime> runtime;
 render::Camera camera;bool close{false},gizmo{false},mouseHeld{false};EntityId selected{0};
 editor::Axis axis{editor::Axis::X};int width{1280},height{720};
 Impl(editor::EditorSession& s,editor::SceneRenderBridge& b,std::unordered_map<render::ResourceId,render::Material>& m,
      render::Camera c,render::DirectionalLight light,const DesktopHostConfig& config)
  :session(&s),bridge(&b),materials(&m),camera(c),width(std::max(1,config.width)),height(std::max(1,config.height)){
  camera.validate();
  if(SDL_InitSubSystem(SDL_INIT_VIDEO)!=0)throw std::runtime_error(std::string("SDL video initialization failed: ")+SDL_GetError());
  try{
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
   window=SDL_CreateWindow(config.title.c_str(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
   if(!window)throw std::runtime_error(std::string("SDL window creation failed: ")+SDL_GetError());
   context=SDL_GL_CreateContext(window);if(!context)throw std::runtime_error(std::string("OpenGL 3.3 context creation failed: ")+SDL_GetError());
   if(SDL_GL_MakeCurrent(window,context)!=0)throw std::runtime_error(std::string("Could not activate OpenGL context: ")+SDL_GetError());
   if(SDL_GL_SetSwapInterval(config.vsync?1:0)!=0)SDL_ClearError();
   int drawableWidth=0,drawableHeight=0;SDL_GL_GetDrawableSize(window,&drawableWidth,&drawableHeight);
   width=std::max(1,drawableWidth);height=std::max(1,drawableHeight);camera.aspectRatio=static_cast<float>(width)/static_cast<float>(height);
   device=std::make_unique<render::gl33::OpenGL33Device>(loadOpenGL());device->setViewport(width,height);
   runtime=std::make_unique<editor::NativeEditorRuntime>(*session,*bridge,*device,*materials,camera,light);
  }catch(...){runtime.reset();device.reset();if(context){SDL_GL_DeleteContext(context);context=nullptr;}if(window){SDL_DestroyWindow(window);window=nullptr;}SDL_QuitSubSystem(SDL_INIT_VIDEO);throw;}
 }
 ~Impl(){runtime.reset();device.reset();if(context){SDL_GL_MakeCurrent(window,context);SDL_GL_DeleteContext(context);}if(window)SDL_DestroyWindow(window);SDL_QuitSubSystem(SDL_INIT_VIDEO);}
 void resize(){int w=0,h=0;SDL_GL_GetDrawableSize(window,&w,&h);width=std::max(1,w);height=std::max(1,h);camera.aspectRatio=static_cast<float>(width)/static_cast<float>(height);device->setViewport(width,height);runtime->setCamera(camera);}
 bool events(){
  SDL_Event event;
  while(SDL_PollEvent(&event)){
   if(event.type==SDL_QUIT)close=true;
   else if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)resize();
   else if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT)mouseHeld=true;
   else if(event.type==SDL_MOUSEBUTTONUP&&event.button.button==SDL_BUTTON_LEFT)mouseHeld=false;
   else if(event.type==SDL_MOUSEMOTION&&mouseHeld&&gizmo&&selected!=0&&session->editScene().contains(selected)){
    const double scale=0.01;const double delta=(axis==editor::Axis::X?event.motion.xrel:(axis==editor::Axis::Y?-event.motion.yrel:event.motion.xrel))*scale;
    try{session->translateAxis(selected,axis,delta);}catch(const std::exception&){gizmo=false;}
   }else if(event.type==SDL_KEYDOWN&&!event.key.repeat){
    const auto key=event.key.keysym.sym;const bool control=(event.key.keysym.mod&KMOD_CTRL)!=0;
    if(key==SDLK_ESCAPE)close=true;
    else if(key==SDLK_g)gizmo=!gizmo;
    else if(key==SDLK_x)axis=editor::Axis::X;
    else if(key==SDLK_y)axis=editor::Axis::Y;
    else if(key==SDLK_z)axis=editor::Axis::Z;
    else if(key==SDLK_F5){if(session->playing())session->stopPlayMode();else session->enterPlayMode();}
    else if(control&&key==SDLK_z)(void)session->undo();
    else if(control&&key==SDLK_y)(void)session->redo();
   }
  }
  return !close;
 }
 render::RenderStats draw(float elapsed){
  const auto* keys=SDL_GetKeyboardState(nullptr);const float dt=std::clamp(elapsed,0.0F,0.1F),speed=4.0F*dt;
  render::Vec3f forward=normalized(camera.forward),right=normalized(cross(forward,camera.up));
  if(keys[SDL_SCANCODE_W])camera.position=add(camera.position,mul(forward,speed));
  if(keys[SDL_SCANCODE_S])camera.position=add(camera.position,mul(forward,-speed));
  if(keys[SDL_SCANCODE_D])camera.position=add(camera.position,mul(right,speed));
  if(keys[SDL_SCANCODE_A])camera.position=add(camera.position,mul(right,-speed));
  if(keys[SDL_SCANCODE_E])camera.position.y+=speed;
  if(keys[SDL_SCANCODE_Q])camera.position.y-=speed;
  runtime->setCamera(camera);const auto stats=runtime->renderFrame();SDL_GL_SwapWindow(window);return stats;
 }
};
NativeDesktopHost::NativeDesktopHost(editor::EditorSession& session,editor::SceneRenderBridge& bridge,
 std::unordered_map<render::ResourceId,render::Material>& materials,render::Camera camera,render::DirectionalLight light,DesktopHostConfig config)
 :impl_(std::make_unique<Impl>(session,bridge,materials,camera,light,config)){}
NativeDesktopHost::~NativeDesktopHost()=default;
void NativeDesktopHost::registerRenderable(EntityId entity,render::ResourceId meshId,render::ResourceId materialId,
 const render::MeshData& mesh,const render::Material& material,render::SphereBounds bounds){
 if(!impl_->session->editScene().contains(entity))throw std::out_of_range("Renderable entity does not exist in the editor scene");
 impl_->device->registerMesh(meshId,mesh);impl_->device->registerMaterial(materialId,material);(*impl_->materials)[materialId]=material;
 impl_->bridge->setRenderComponents(entity,{meshId,materialId,bounds,true,{}});
}
void NativeDesktopHost::registerTexture(render::ResourceId textureId,render::ResourceId materialId,const assets::ImageRgba8& image){
 if(image.width==0||image.height==0||image.pixels.size()!=static_cast<std::size_t>(image.width)*image.height*4U)throw std::invalid_argument("Texture must contain tightly packed RGBA8 pixels");
 impl_->device->uploadRgba8(textureId,image.width,image.height,image.pixels.data(),image.pixels.size());impl_->device->bindBaseColorTexture(materialId,textureId);
}
void NativeDesktopHost::registerTextureFile(render::ResourceId textureId,render::ResourceId materialId,const std::string& path){
 std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Could not open texture file: "+path);
 std::vector<char> raw((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());std::vector<std::byte> bytes;bytes.reserve(raw.size());
 for(unsigned char c:raw)bytes.push_back(static_cast<std::byte>(c));registerTexture(textureId,materialId,assets::decodeImage(bytes));
}
bool NativeDesktopHost::pumpEvents(){return impl_->events();}
render::RenderStats NativeDesktopHost::frame(float elapsedSeconds){(void)impl_->events();return impl_->draw(elapsedSeconds);}
void NativeDesktopHost::setSelectedEntity(EntityId entity)noexcept{impl_->selected=entity;}
bool NativeDesktopHost::shouldClose()const noexcept{return impl_->close;}
void* NativeDesktopHost::nativeWindow()const noexcept{return impl_->window;}
}
#endif

#include "mid/lua_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#ifdef MID_ENGINE_WITH_LUA
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#endif
namespace mid::script {
#ifdef MID_ENGINE_WITH_LUA
struct LuaScriptInstance::Impl {
 struct Memory { std::size_t used{0},limit{0}; } memory;
 lua_State* state{nullptr}; Scene* activeScene{nullptr}; LuaConfig config{};
 std::size_t instructions{0}; std::string lastError; std::vector<std::string> logs; std::vector<int> breakpoints; std::optional<int> breakpointLine;
 static void* allocate(void* user,void* ptr,std::size_t oldSize,std::size_t newSize){
  auto& m=*static_cast<Memory*>(user);const std::size_t old=ptr?oldSize:0;
  if(newSize==0){if(ptr)std::free(ptr);m.used=m.used>=old?m.used-old:0;return nullptr;}
  if(newSize>m.limit||m.used-old>m.limit-newSize)return nullptr;
  void* next=std::realloc(ptr,newSize);if(next)m.used=m.used-old+newSize;return next;
 }
 static Impl* self(lua_State* L){lua_getfield(L,LUA_REGISTRYINDEX,"mid.engine.instance");auto* p=static_cast<Impl*>(lua_touserdata(L,-1));lua_pop(L,1);return p;}
 static int setPosition(lua_State* L){
  auto* p=self(L);if(!p||!p->activeScene)return luaL_error(L,"scene mutation is only allowed inside a lifecycle callback");
  const auto id=luaL_checkinteger(L,1);const double x=luaL_checknumber(L,2),y=luaL_checknumber(L,3),z=luaL_checknumber(L,4);
  if(id<=0||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return luaL_error(L,"set_position expects a positive entity ID and finite coordinates");
  if(!p->activeScene->contains(static_cast<EntityId>(id)))return luaL_error(L,"set_position entity does not exist");
  try{auto t=p->activeScene->get(static_cast<EntityId>(id)).transform;t.position={x,y,z};p->activeScene->setTransform(static_cast<EntityId>(id),t);}
  catch(const std::exception& e){p->lastError=e.what();return luaL_error(L,"%s",p->lastError.c_str());}
  return 0;
 }
 static int setActive(lua_State* L){
  auto* p=self(L);if(!p||!p->activeScene)return luaL_error(L,"scene mutation is only allowed inside a lifecycle callback");
  const auto id=luaL_checkinteger(L,1);const bool active=lua_toboolean(L,2)!=0;
  if(id<=0||!p->activeScene->contains(static_cast<EntityId>(id)))return luaL_error(L,"set_active entity does not exist");
  p->activeScene->get(static_cast<EntityId>(id)).active=active;return 0;
 }
 static int log(lua_State* L){auto* p=self(L);if(!p)return 0;std::size_t n=0;const char* s=luaL_tolstring(L,1,&n);p->logs.emplace_back(s,n);lua_pop(L,1);return 0;}
 static void hook(lua_State* L,lua_Debug* ar){
  auto* p=self(L);if(!p)return;
  if(ar->event==LUA_HOOKCOUNT){p->instructions+=1000;if(p->instructions>p->config.instructionBudget)luaL_error(L,"instruction budget exceeded");}
  else if(ar->event==LUA_HOOKLINE){lua_getinfo(L,"l",ar);if(std::find(p->breakpoints.begin(),p->breakpoints.end(),ar->currentline)!=p->breakpoints.end()){p->breakpointLine=ar->currentline;luaL_error(L,"debugger breakpoint");}}
 }
 void removeGlobal(const char* name){lua_pushnil(state);lua_setglobal(state,name);}
};
#else
struct LuaScriptInstance::Impl {};
#endif
bool LuaScriptInstance::available() noexcept {
#ifdef MID_ENGINE_WITH_LUA
 return true;
#else
 return false;
#endif
}
LuaScriptInstance::LuaScriptInstance(std::string source,LuaConfig config):impl_(std::make_unique<Impl>()){
 if(config.memoryLimitBytes<1024||config.instructionBudget<1000)throw std::invalid_argument("Lua limits are too small to run safely");
#ifndef MID_ENGINE_WITH_LUA
 (void)source;(void)config;throw std::runtime_error("Lua support is not built. Install Lua development headers/libraries and reconfigure CMake.");
#else
 impl_->config=config;impl_->memory.limit=config.memoryLimitBytes;
 impl_->state=lua_newstate(&Impl::allocate,&impl_->memory);if(!impl_->state)throw std::runtime_error("Unable to allocate bounded Lua state");
 lua_pushlightuserdata(impl_->state,impl_.get());lua_setfield(impl_->state,LUA_REGISTRYINDEX,"mid.engine.instance");
 luaL_openlibs(impl_->state);
 for(const char* name:{"io","os","package","debug","dofile","loadfile","load","collectgarbage","require"})impl_->removeGlobal(name);
 lua_pushlightuserdata(impl_->state,impl_.get());lua_pushcclosure(impl_->state,&Impl::setPosition,1);lua_setglobal(impl_->state,"set_position");
 lua_pushlightuserdata(impl_->state,impl_.get());lua_pushcclosure(impl_->state,&Impl::setActive,1);lua_setglobal(impl_->state,"set_active");
 lua_pushlightuserdata(impl_->state,impl_.get());lua_pushcclosure(impl_->state,&Impl::log,1);lua_setglobal(impl_->state,"log");
 lua_sethook(impl_->state,&Impl::hook,LUA_MASKCOUNT|LUA_MASKLINE,1000);
 if(luaL_loadbuffer(impl_->state,source.data(),source.size(),"@mid-script")!=LUA_OK||lua_pcall(impl_->state,0,0,0)!=LUA_OK){std::string error=lua_tostring(impl_->state,-1)?lua_tostring(impl_->state,-1):"Lua compile/runtime error";lua_close(impl_->state);impl_->state=nullptr;throw std::runtime_error("Lua script load failed: "+error);}
 lua_sethook(impl_->state,nullptr,0,0);
#endif
}
LuaScriptInstance::~LuaScriptInstance(){
#ifdef MID_ENGINE_WITH_LUA
 if(impl_&&impl_->state)lua_close(impl_->state);
#endif
}
LuaScriptInstance::LuaScriptInstance(LuaScriptInstance&&) noexcept=default;
LuaScriptInstance& LuaScriptInstance::operator=(LuaScriptInstance&&) noexcept=default;
LuaResult LuaScriptInstance::start(Scene& scene){return invoke("start",scene,0,false);}
LuaResult LuaScriptInstance::update(Scene& scene,float delta){return invoke("update",scene,delta,true);}
LuaResult LuaScriptInstance::stop(Scene& scene){return invoke("stop",scene,0,false);}
void LuaScriptInstance::setBreakpoints(std::vector<int> lines){
 for(int line:lines)if(line<1)throw std::invalid_argument("Breakpoint line numbers must be positive");
 if(!impl_)throw std::logic_error("Lua script instance was moved from");
#ifdef MID_ENGINE_WITH_LUA
 impl_->breakpoints=std::move(lines);
#else
 (void)lines;
#endif
}
LuaResult LuaScriptInstance::invoke(const char* callback,Scene& scene,float deltaSeconds,bool passDelta){
 if(!std::isfinite(deltaSeconds)||deltaSeconds<0)throw std::invalid_argument("Script delta time must be finite and non-negative");
 if(!impl_)throw std::logic_error("Lua script instance was moved from");
#ifndef MID_ENGINE_WITH_LUA
 (void)callback;(void)scene;(void)passDelta;throw std::runtime_error("Lua support is not built");
#else
 LuaResult result;Scene candidate=scene;impl_->activeScene=&candidate;impl_->instructions=0;impl_->logs.clear();impl_->breakpointLine.reset();impl_->lastError.clear();
 lua_getglobal(impl_->state,callback);
 if(!lua_isfunction(impl_->state,-1)){lua_pop(impl_->state,1);impl_->activeScene=nullptr;result.completed=true;return result;}
 if(passDelta)lua_pushnumber(impl_->state,deltaSeconds);
 lua_sethook(impl_->state,&Impl::hook,LUA_MASKCOUNT|LUA_MASKLINE,1000);
 const int status=lua_pcall(impl_->state,passDelta?1:0,0,0);
 lua_sethook(impl_->state,nullptr,0,0);impl_->activeScene=nullptr;
 result.instructions=impl_->instructions;result.logs=impl_->logs;result.breakpointLine=impl_->breakpointLine;
 if(status!=LUA_OK){const char* message=lua_tostring(impl_->state,-1);result.error=message?message:"Lua callback failed";lua_pop(impl_->state,1);return result;}
 scene=std::move(candidate);result.completed=true;return result;
#endif
}
}

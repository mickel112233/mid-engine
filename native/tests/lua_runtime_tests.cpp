#include "mid/lua_runtime.hpp"
#include <iostream>
#include <stdexcept>
int main(){
 int failures=0;auto check=[&](bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';++failures;}};
 if(!mid::script::LuaScriptInstance::available()){
  bool rejected=false;try{mid::script::LuaScriptInstance script("function start() end");}catch(const std::runtime_error&){rejected=true;}
  check(rejected,"missing optional Lua dependency fails with a clear error");
  return failures?1:0;
 }
 mid::Scene scene("Lua test");const auto id=scene.createEntity("actor");
 mid::script::LuaScriptInstance script("function start() set_position(1,1,2,3); log('started') end\nfunction update(dt) set_position(1,1+dt,2,3) end\nfunction stop() set_active(1,false) end");
 auto started=script.start(scene);check(started.completed&&scene.get(id).transform.position.x==1.0,"Lua start callback mutates scene through safe API");
 check(!started.logs.empty(),"Lua log callback captures script messages");
 auto updated=script.update(scene,0.5F);check(updated.completed&&scene.get(id).transform.position.x==1.5,"Lua update callback receives delta time");
 auto stopped=script.stop(scene);check(stopped.completed&&!scene.get(id).active,"Lua stop callback runs");
 mid::script::LuaConfig limited;limited.instructionBudget=1000;
 mid::script::LuaScriptInstance infinite("function update() while true do end end",limited);
 const auto before=scene.get(id).transform.position.x;auto failed=infinite.update(scene,0.01F);
 check(!failed.completed&&!failed.error.empty()&&scene.get(id).transform.position.x==before,"Lua instruction budget aborts runaway script atomically");
 return failures?1:0;
}

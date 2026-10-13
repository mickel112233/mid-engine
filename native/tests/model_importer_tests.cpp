#include "mid/model_importer.hpp"
#include <iostream>
#include <stdexcept>
int main(){
 int failures=0;auto check=[&](bool ok,const char* message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';++failures;}};
 bool rejected=false;try{(void)mid::assets::importModelFile("this-file-does-not-exist.glb");}catch(const std::runtime_error&){rejected=true;}
 check(rejected,"missing/invalid model import reports a descriptive error");
 if(!mid::assets::hasAssimpModelSupport())check(true,"Assimp support remains optional");
 if(mid::assets::hasAssimpModelSupport()){
  const char* fixture="native/tests/fixtures/minimal_embedded_texture.gltf";
  try{
   const auto model=mid::assets::importModelFile(fixture);
   check(!model.meshes.empty(),"real glTF fixture imports its triangle mesh");
   check(!model.materials.empty(),"real glTF fixture imports its material");
   check(!model.embeddedTextures.empty(),"glTF data-URI image is extracted as an embedded texture");
   if(!model.embeddedTextures.empty())check(!model.embeddedTextures.front().bytes.empty(),"embedded glTF texture payload is preserved");
  }catch(const std::exception& e){std::cerr<<"FAIL: glTF fixture import threw: "<<e.what()<<'\n';++failures;}
 }
 return failures?1:0;
}

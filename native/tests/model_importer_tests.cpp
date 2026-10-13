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
   if(!model.embeddedTextures.empty()){
    check(!model.embeddedTextures.front().bytes.empty(),"embedded glTF texture payload is preserved");
    check(model.embeddedTextures.front().compressed,"PNG image payload remains encoded until image-decoder upload");
    check(model.embeddedTextures.front().formatHint=="png","embedded image format hint is retained");
    try {
     const auto decoded=mid::assets::decodeImportedTexture(model.embeddedTextures.front());
     check(decoded.width==1&&decoded.height==1&&decoded.pixels.size()==4,"embedded PNG is decoded into upload-ready RGBA8 pixels");
    } catch(const std::exception& e) {
     std::cerr<<"FAIL: embedded PNG decode threw: "<<e.what()<<'\\n';++failures;
    }
   }
   check(model.meshMaterialIndices.size()==model.meshes.size(),"each imported mesh retains its source material index");
   check(!model.meshMaterialIndices.empty()&&model.meshMaterialIndices.front()<model.materials.size(),"imported mesh material index points at an imported material");
   check(!model.materials.empty()&&!model.materials.front().texturePaths.empty()&&model.materials.front().texturePaths.front()=="*0","material preserves its embedded-texture reference for later binding");
   check(!model.nodes.empty()&&!model.nodes.front().meshIndices.empty(),"scene hierarchy retains node-to-mesh associations");
   check(!model.meshes.empty()&&model.meshes.front().vertices.size()==3&&model.meshes.front().indices.size()==3,"glTF fixture imports its indexed triangle geometry");
  }catch(const std::exception& e){std::cerr<<"FAIL: glTF fixture import threw: "<<e.what()<<'\n';++failures;}
 }
 return failures?1:0;
}

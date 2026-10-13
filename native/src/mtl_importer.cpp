#include "mid/mtl_importer.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace mid::assets {
std::unordered_map<std::string,ImportedMaterial> importMtl(std::string_view source){
 std::unordered_map<std::string,ImportedMaterial> result;std::istringstream input{std::string(source)};std::string line;ImportedMaterial* current=nullptr;
 while(std::getline(input,line)){std::istringstream row(line);std::string key;if(!(row>>key)||key.starts_with('#'))continue;
  if(key=="newmtl"){std::string name;if(!(row>>name)||name.empty())throw std::invalid_argument("MTL material name is missing");auto [it,inserted]=result.try_emplace(name);if(!inserted)throw std::invalid_argument("Duplicate MTL material: "+name);it->second.material.name=name;current=&it->second;}
  else if(current&&key=="Kd"){if(!(row>>current->material.baseColor.x>>current->material.baseColor.y>>current->material.baseColor.z))throw std::invalid_argument("Invalid MTL diffuse color");}
  else if(current&&key=="d"){float alpha;if(!(row>>alpha)||!std::isfinite(alpha))throw std::invalid_argument("Invalid MTL opacity");current->material.baseColor.w=std::clamp(alpha,0.0F,1.0F);current->material.transparent=alpha<0.999F;}
  else if(current&&key=="Tr"){float transparency;if(!(row>>transparency)||!std::isfinite(transparency))throw std::invalid_argument("Invalid MTL transparency");current->material.baseColor.w=1-std::clamp(transparency,0.0F,1.0F);current->material.transparent=transparency>0.001F;}
  else if(current&&key=="Ns"){float shininess;if(!(row>>shininess)||!std::isfinite(shininess))throw std::invalid_argument("Invalid MTL shininess");current->material.roughness=std::clamp(std::sqrt(2.0F/(std::max(0.0F,shininess)+2.0F)),0.04F,1.0F);}
  else if(current&&key=="map_Kd"){std::string path;if(!(row>>path))throw std::invalid_argument("MTL diffuse texture path is missing");current->baseColorTexture=path;}
  else if(current&&key=="illum"){int model;if(!(row>>model))throw std::invalid_argument("Invalid MTL illumination model");current->material.metallic=(model==5||model==7)?0.5F:0.0F;}
 }
 for(const auto& [name,material]:result){(void)name;material.material.validate();}
 return result;
}
}

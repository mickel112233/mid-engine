#include "mid/mtl_importer.hpp"
#include <iostream>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}};
 const auto materials=mid::assets::importMtl("newmtl Painted\nKd 0.2 0.4 0.8\nNs 64\nd 0.5\nmap_Kd textures/paint.png\n");
 check(materials.size()==1,"MTL material is created");const auto& m=materials.at("Painted");
 check(m.material.baseColor.z>0.79F&&m.material.baseColor.w==0.5F,"diffuse color and opacity are imported");
 check(m.material.transparent,"transparency is recognized");check(m.baseColorTexture=="textures/paint.png","diffuse texture reference is imported");
 if(failures)return 1;std::cout<<"MTL importer tests passed\n";return 0;
}

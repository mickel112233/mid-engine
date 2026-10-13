#include "mid/pbr.hpp"
#include <cmath>
#include <iostream>
int main(){
 int failures=0;auto check=[&](bool ok,const char* m){if(!ok){std::cerr<<"FAIL: "<<m<<'\n';++failures;}};
 mid::render::pbr::Surface surface;mid::render::pbr::Lighting light;light.viewDirection={0,1,1};
 const auto lit=mid::render::pbr::evaluate(surface,light);
 check(std::isfinite(lit.x)&&std::isfinite(lit.y)&&std::isfinite(lit.z),"PBR output is finite");
 check(lit.x>0&&lit.y>0&&lit.z>0,"direct and ambient lighting contribute");
 light.lightDirection={0,-1,0};const auto back=mid::render::pbr::evaluate(surface,light);
 check(std::isfinite(back.x),"back-facing light remains numerically stable");
 bool rejected=false;try{surface.metallic=2;(void)mid::render::pbr::evaluate(surface,light);}catch(...){rejected=true;}
 check(rejected,"invalid PBR factors are rejected");
 if(failures)return 1;std::cout<<"PBR tests passed\n";return 0;
}
